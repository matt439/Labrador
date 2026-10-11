#include "engine/render/block_decode.h"

#include "engine/render/texture_format.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace labrador
{
	namespace
	{
		// One texel, in the order the result stores it.
		struct Texel
		{
			unsigned char red = 0;
			unsigned char green = 0;
			unsigned char blue = 0;
			unsigned char alpha = 0;
		};

		unsigned int read_u16(const unsigned char* bytes)
		{
			return static_cast<unsigned int>(bytes[0]) |
				(static_cast<unsigned int>(bytes[1]) << 8);
		}

		unsigned int read_u32(const unsigned char* bytes)
		{
			return read_u16(bytes) | (read_u16(bytes + 2) << 16);
		}

		// A 5:6:5 endpoint widened to eight bits a channel by repeating its
		// high bits, which sends a channel's zero to 0 and its maximum to 255.
		Texel expand_565(unsigned int colour)
		{
			const unsigned int red = (colour >> 11) & 31u;
			const unsigned int green = (colour >> 5) & 63u;
			const unsigned int blue = colour & 31u;
			Texel texel;
			texel.red = static_cast<unsigned char>((red << 3) | (red >> 2));
			texel.green = static_cast<unsigned char>((green << 2) | (green >> 4));
			texel.blue = static_cast<unsigned char>((blue << 3) | (blue >> 2));
			texel.alpha = 255;
			return texel;
		}

		// `weight_a` parts of `a` and `weight_b` parts of `b`, out of their sum,
		// rounded to nearest.
		unsigned char blend(unsigned int a, unsigned int b,
			unsigned int weight_a, unsigned int weight_b)
		{
			const unsigned int total = weight_a + weight_b;
			return static_cast<unsigned char>(
				(a * weight_a + b * weight_b + total / 2) / total);
		}

		Texel blend(const Texel& a, const Texel& b, unsigned int weight_a,
			unsigned int weight_b)
		{
			Texel texel;
			texel.red = blend(a.red, b.red, weight_a, weight_b);
			texel.green = blend(a.green, b.green, weight_a, weight_b);
			texel.blue = blend(a.blue, b.blue, weight_a, weight_b);
			texel.alpha = 255;
			return texel;
		}

		// The eight bytes every one of the three formats ends a block with:
		// two 5:6:5 endpoints and sixteen two-bit indices, texel 0 in the low
		// bits. `three_colour_allowed` is BC1's alone - see the header for
		// which block chooses which mode.
		void decode_colour(const unsigned char* bytes,
			bool three_colour_allowed, Texel out[16])
		{
			const unsigned int first = read_u16(bytes);
			const unsigned int second = read_u16(bytes + 2);
			const unsigned int indices = read_u32(bytes + 4);

			Texel palette[4];
			palette[0] = expand_565(first);
			palette[1] = expand_565(second);
			if (!three_colour_allowed || first > second)
			{
				palette[2] = blend(palette[0], palette[1], 2, 1);
				palette[3] = blend(palette[0], palette[1], 1, 2);
			}
			else
			{
				palette[2] = blend(palette[0], palette[1], 1, 1);
				palette[3] = Texel();
			}

			for (int texel = 0; texel < 16; texel++)
			{
				out[texel] = palette[(indices >> (2 * texel)) & 3u];
			}
		}

		// BC2's alpha half: sixteen four-bit values, two a byte, low nibble
		// first.
		void decode_explicit_alpha(const unsigned char* bytes, Texel out[16])
		{
			for (int texel = 0; texel < 16; texel++)
			{
				const unsigned int byte = bytes[texel / 2];
				const unsigned int nibble = (texel % 2 == 0)
					? (byte & 15u) : (byte >> 4);
				out[texel].alpha = static_cast<unsigned char>(nibble * 17u);
			}
		}

		// BC3's alpha half: two eight-bit endpoints, then sixteen three-bit
		// indices packed into six bytes, texel 0 in the low bits.
		void decode_interpolated_alpha(const unsigned char* bytes,
			Texel out[16])
		{
			const unsigned int first = bytes[0];
			const unsigned int second = bytes[1];

			unsigned char palette[8];
			palette[0] = static_cast<unsigned char>(first);
			palette[1] = static_cast<unsigned char>(second);
			if (first > second)
			{
				for (unsigned int i = 1; i < 7; i++)
				{
					palette[i + 1] = blend(first, second, 7 - i, i);
				}
			}
			else
			{
				for (unsigned int i = 1; i < 5; i++)
				{
					palette[i + 1] = blend(first, second, 5 - i, i);
				}
				palette[6] = 0;
				palette[7] = 255;
			}

			std::uint64_t indices = 0;
			for (int byte = 0; byte < 6; byte++)
			{
				indices |= static_cast<std::uint64_t>(bytes[2 + byte])
					<< (8 * byte);
			}
			for (int texel = 0; texel < 16; texel++)
			{
				out[texel].alpha = palette[(indices >> (3 * texel)) & 7u];
			}
		}

		void decode_block(TextureFormat format, const unsigned char* block,
			Texel out[16])
		{
			switch (format)
			{
			case TextureFormat::bc1_unorm:
				decode_colour(block, true, out);
				break;
			case TextureFormat::bc2_unorm:
				decode_colour(block + 8, false, out);
				decode_explicit_alpha(block, out);
				break;
			case TextureFormat::bc3_unorm:
			default:
				decode_colour(block + 8, false, out);
				decode_interpolated_alpha(block, out);
				break;
			}
		}

		std::string level_error(size_t index, const std::string& why)
		{
			return "decode_block_compressed: level " + std::to_string(index) +
				" " + why;
		}
	}

	TextureData decode_block_compressed(const TextureData& texture)
	{
		if (!is_block_compressed(texture.format))
		{
			throw std::runtime_error("decode_block_compressed: the texture is "
				"not BC1, BC2 or BC3, so there are no blocks to decode.");
		}
		if (texture.levels.empty())
		{
			throw std::runtime_error("decode_block_compressed: the texture has "
				"no levels.");
		}

		const size_t block_bytes =
			texture.format == TextureFormat::bc1_unorm ? 8u : 16u;

		TextureData decoded;
		decoded.width = texture.width;
		decoded.height = texture.height;
		decoded.format = TextureFormat::r8g8b8a8_unorm;

		// The layout first and the bytes second, so that a texture refused
		// for its last level has not had its first decoded for nothing.
		size_t decoded_size = 0;
		for (size_t i = 0; i < texture.levels.size(); i++)
		{
			const TextureLevel& level = texture.levels[i];
			const TextureLevel expected = texture_level(texture.format,
				level.width, level.height, level.offset,
				"decode_block_compressed");
			if (level.stride != expected.stride || level.rows != expected.rows ||
				level.size != expected.size)
			{
				throw std::runtime_error(level_error(i, "is not laid out as "
					"4x4 blocks of its size."));
			}
			if (level.offset > texture.pixels.size() ||
				texture.pixels.size() - level.offset < level.size)
			{
				throw std::runtime_error(level_error(i, "runs past the end of "
					"the texture's bytes."));
			}

			const TextureLevel out = texture_level(decoded.format,
				level.width, level.height, decoded_size,
				"decode_block_compressed");
			decoded.levels.push_back(out);
			decoded_size = out.offset + out.size;
		}
		decoded.pixels.resize(decoded_size);

		for (size_t i = 0; i < texture.levels.size(); i++)
		{
			const TextureLevel& level = texture.levels[i];
			const TextureLevel& out = decoded.levels[i];
			const size_t columns = static_cast<size_t>(level.stride) /
				block_bytes;

			for (int block_y = 0; block_y < level.rows; block_y++)
			{
				for (size_t block_x = 0; block_x < columns; block_x++)
				{
					const unsigned char* block = texture.pixels.data() +
						level.offset +
						static_cast<size_t>(block_y) *
							static_cast<size_t>(level.stride) +
						block_x * block_bytes;

					Texel texels[16];
					decode_block(texture.format, block, texels);

					for (int texel = 0; texel < 16; texel++)
					{
						const size_t x = block_x * 4 +
							static_cast<size_t>(texel % 4);
						const size_t y = static_cast<size_t>(block_y) * 4 +
							static_cast<size_t>(texel / 4);
						if (x >= static_cast<size_t>(level.width) ||
							y >= static_cast<size_t>(level.height))
						{
							continue;
						}

						unsigned char* pixel = decoded.pixels.data() +
							out.offset +
							y * static_cast<size_t>(out.stride) + x * 4;
						pixel[0] = texels[texel].red;
						pixel[1] = texels[texel].green;
						pixel[2] = texels[texel].blue;
						pixel[3] = texels[texel].alpha;
					}
				}
			}
		}

		return decoded;
	}
}

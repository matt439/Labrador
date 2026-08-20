#include "engine/render/dds_file.h"

#include "engine/core/byte_reader.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace labrador
{
	namespace
	{
		const uint32_t MAGIC = 0x20534444;          // "DDS "
		const uint32_t HEADER_SIZE = 124;
		const uint32_t PIXEL_FORMAT_SIZE = 32;

		// DDS_PIXELFORMAT flags, and the two caps bits that say a file is not
		// the one kind of texture this engine draws.
		const uint32_t FOURCC = 0x4;
		const uint32_t RGB = 0x40;
		const uint32_t CUBEMAP = 0x200;
		const uint32_t VOLUME = 0x200000;

		std::string hex(uint32_t value)
		{
			const char* digits = "0123456789ABCDEF";
			std::string text = "0x";
			for (int shift = 28; shift >= 0; shift -= 4)
			{
				text += digits[(value >> shift) & 0xFu];
			}
			return text;
		}

		// A fourCC, printably. It is four ASCII characters in every file that
		// has one, and is worth showing as such - "DXT2" is a thing somebody can
		// look up and 0x32545844 is not.
		std::string four_cc(uint32_t value)
		{
			std::string text;
			for (int shift = 0; shift <= 24; shift += 8)
			{
				const char character =
					static_cast<char>((value >> shift) & 0xFFu);
				text += (character >= ' ' && character <= '~')
					? character : '?';
			}
			return text;
		}

		uint32_t make_four_cc(char a, char b, char c, char d)
		{
			return static_cast<uint32_t>(static_cast<unsigned char>(a)) |
				(static_cast<uint32_t>(static_cast<unsigned char>(b)) << 8) |
				(static_cast<uint32_t>(static_cast<unsigned char>(c)) << 16) |
				(static_cast<uint32_t>(static_cast<unsigned char>(d)) << 24);
		}

		// The whole of what a .dds says about its format, in one place.
		//
		// DXT2 AND DXT4 CARRY PREMULTIPLIED ALPHA AND DXT3 AND DXT5 DO NOT, and
		// there is no format here that says which - the block layouts are
		// identical and DXGI has never distinguished them, so the pair collapse.
		// That is not a shortcut: this engine's blend equation IS premultiplied
		// (RenderPixelTests pins the source factor at ONE), so every one of
		// these files is expected to be premultiplied whatever its fourCC
		// claims, and the fourCC is a label on the content rather than an
		// instruction to the renderer.
		TextureFormat format_of(uint32_t flags, uint32_t fourcc,
			uint32_t bit_count, uint32_t red, uint32_t green, uint32_t blue,
			uint32_t alpha, const std::string& path)
		{
			if ((flags & FOURCC) != 0)
			{
				if (fourcc == make_four_cc('D', 'X', 'T', '1'))
				{
					return TextureFormat::bc1_unorm;
				}
				if (fourcc == make_four_cc('D', 'X', 'T', '2') ||
					fourcc == make_four_cc('D', 'X', 'T', '3'))
				{
					return TextureFormat::bc2_unorm;
				}
				if (fourcc == make_four_cc('D', 'X', 'T', '4') ||
					fourcc == make_four_cc('D', 'X', 'T', '5'))
				{
					return TextureFormat::bc3_unorm;
				}
				if (fourcc == make_four_cc('D', 'X', '1', '0'))
				{
					throw std::runtime_error(path + " uses the DX10 extended "
						"header, which this engine does not read. Nothing in "
						"either client writes one; re-export it as a plain "
						"DXT1, DXT3 or DXT5 file.");
				}
				throw std::runtime_error(path + " is in fourCC format \"" +
					four_cc(fourcc) + "\", which this engine does not read "
					"(texture_format.h).");
			}

			if ((flags & RGB) != 0 && bit_count == 32)
			{
				if (red == 0x000000FFu && green == 0x0000FF00u &&
					blue == 0x00FF0000u && alpha == 0xFF000000u)
				{
					return TextureFormat::r8g8b8a8_unorm;
				}
				if (red == 0x00FF0000u && green == 0x0000FF00u &&
					blue == 0x000000FFu && alpha == 0xFF000000u)
				{
					return TextureFormat::b8g8r8a8_unorm;
				}
				throw std::runtime_error(path + " is a 32-bit image with "
					"channel masks r=" + hex(red) + " g=" + hex(green) +
					" b=" + hex(blue) + " a=" + hex(alpha) + ", which is "
					"neither RGBA nor BGRA. An image with no alpha channel is "
					"the likeliest way to get here.");
			}

			throw std::runtime_error(path + " stores " +
				std::to_string(bit_count) + " bits a pixel with pixel-format "
				"flags " + hex(flags) + ", which this engine does not read "
				"(texture_format.h).");
		}

		// How many levels a chain from these dimensions can have: the larger
		// side halved until it is one, counted as you go. Level zero is the
		// first, so a 1x1 texture has one.
		uint32_t full_chain_length(int width, int height)
		{
			uint32_t levels = 1;
			for (int side = width > height ? width : height; side > 1;
				side /= 2)
			{
				levels++;
			}
			return levels;
		}
	}

	TextureData read_dds_file(const std::string& path)
	{
		const std::vector<unsigned char> bytes = read_file_bytes(path);
		ByteReader reader(bytes, path);

		if (reader.read_u32() != MAGIC)
		{
			throw std::runtime_error(path +
				" is not a .dds: it does not start with \"DDS \".");
		}

		// The header is a fixed 124 bytes and says so in its own first field,
		// which is the cheapest way to catch a file that is a .dds by extension
		// and something else by content.
		const uint32_t header_size = reader.read_u32();
		if (header_size != HEADER_SIZE)
		{
			throw std::runtime_error(path + " declares a " +
				std::to_string(header_size) + " byte header; a .dds header is " +
				std::to_string(HEADER_SIZE) + " bytes.");
		}

		std::ignore = reader.read_u32();                       // flags
		const int height = static_cast<int>(reader.read_u32());
		const int width = static_cast<int>(reader.read_u32());
		std::ignore = reader.read_u32();                       // pitch or size
		const uint32_t depth = reader.read_u32();
		const uint32_t mip_count = reader.read_u32();
		reader.skip(11 * 4);                                   // reserved

		const uint32_t pixel_format_size = reader.read_u32();
		if (pixel_format_size != PIXEL_FORMAT_SIZE)
		{
			throw std::runtime_error(path + " declares a " +
				std::to_string(pixel_format_size) + " byte pixel format; a "
				".dds pixel format is " + std::to_string(PIXEL_FORMAT_SIZE) +
				" bytes.");
		}

		const uint32_t pixel_flags = reader.read_u32();
		const uint32_t fourcc = reader.read_u32();
		const uint32_t bit_count = reader.read_u32();
		const uint32_t red = reader.read_u32();
		const uint32_t green = reader.read_u32();
		const uint32_t blue = reader.read_u32();
		const uint32_t alpha = reader.read_u32();

		std::ignore = reader.read_u32();                       // caps
		const uint32_t caps2 = reader.read_u32();
		reader.skip(3 * 4);                          // caps3, caps4, reserved2

		if (width <= 0 || height <= 0)
		{
			throw std::runtime_error(path + " describes an image of " +
				std::to_string(width) + "x" + std::to_string(height) +
				", which is not an image.");
		}

		// EACH OF THESE IS A DIFFERENT TEXTURE, not a variation on this one. A
		// cube map is six faces, a volume is a stack of slices, and a caller
		// asking for either through load_texture_asset wants something this
		// engine has no way to draw - so it is told, rather than handed the
		// first face and left to wonder why the other five never appear.
		if ((caps2 & CUBEMAP) != 0)
		{
			throw std::runtime_error(path + " is a cube map. This engine draws "
				"one face of one 2D texture and has nowhere to put the other "
				"five.");
		}
		if ((caps2 & VOLUME) != 0 || depth > 1)
		{
			throw std::runtime_error(path + " is a volume texture, " +
				std::to_string(depth) + " slices deep. This engine draws 2D.");
		}

		TextureData texture;
		texture.width = width;
		texture.height = height;
		texture.format = format_of(pixel_flags, fourcc, bit_count, red, green,
			blue, alpha, path);

		// A mip count of zero means one level: the field is only meaningful
		// when the file says it has a chain, and a writer that has none leaves
		// it at zero rather than at one.
		const uint32_t levels = mip_count < 1 ? 1u : mip_count;

		// AND A CHAIN CANNOT BE LONGER THAN THE DIMENSIONS ALLOW. Halving the
		// larger side until it reaches one is what a chain is, so a file
		// claiming more levels than that is claiming levels that cannot exist -
		// and the loop below would build every one of them, all 1x1, for
		// whatever number the file said. That is a header field deciding how
		// much this process allocates and how many subresources every backend
		// downstream is then asked about, which is worth refusing by name
		// rather than surviving (T6).
		//
		// HERE RATHER THAN IN A BACKEND, because every backend inherits this
		// list and the count is a fact about the file rather than about an API.
		// A backend with a limit of its own still states it - D3D12's is 15 and
		// its texture factory says so - but it is stating a second wall, not
		// the first.
		const uint32_t possible = full_chain_length(width, height);
		if (levels > possible)
		{
			throw std::runtime_error(path + " says it has " +
				std::to_string(levels) + " mip levels. A " +
				std::to_string(width) + "x" + std::to_string(height) +
				" texture has at most " + std::to_string(possible) + ".");
		}

		int level_width = width;
		int level_height = height;
		size_t offset = 0;
		for (uint32_t i = 0; i < levels; i++)
		{
			const TextureLevel level = texture_level(texture.format,
				level_width, level_height, offset);
			texture.levels.push_back(level);
			offset += level.size;

			level_width = level_width > 1 ? level_width / 2 : 1;
			level_height = level_height > 1 ? level_height / 2 : 1;
		}

		// The rest of the file is the levels, back to back, and there must be
		// enough of it. Anything after them is left alone - a .dds written by a
		// tool that appends its own metadata is still a .dds.
		texture.pixels = reader.read_bytes(offset);

		return texture;
	}
}

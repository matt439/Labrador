#include <doctest/doctest.h>

#include "engine/render/block_decode.h"
#include "engine/render/sprite_font_file.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <vector>

// Decoding BC1, BC2 and BC3 to pixels, with no device.
//
// AGAINST BLOCKS BUILT HERE, each one chosen so that the texel it decodes to
// is a number that can be worked out by hand and is written beside it. Every
// interpolated value below is the rounded-to-nearest one block_decode.h
// promises, so a decoder that truncated instead, or took BC1's three-colour
// mode for a BC2 or BC3 block, or packed BC3's three-bit indices from the
// wrong end, fails on a named texel rather than on a picture.

namespace
{
	using labrador::TextureData;
	using labrador::TextureFormat;
	using labrador::TextureLevel;
	using labrador::decode_block_compressed;
	using labrador::texture_level;

	struct Rgba
	{
		int red;
		int green;
		int blue;
		int alpha;

		bool operator==(const Rgba& other) const
		{
			return red == other.red && green == other.green &&
				blue == other.blue && alpha == other.alpha;
		}
	};

	std::ostream& operator<<(std::ostream& out, const Rgba& texel)
	{
		return out << texel.red << "," << texel.green << "," << texel.blue <<
			"," << texel.alpha;
	}

	const unsigned int RED_565 = 0xF800;
	const unsigned int GREEN_565 = 0x07E0;
	const unsigned int BLUE_565 = 0x001F;
	const unsigned int WHITE_565 = 0xFFFF;
	const unsigned int BLACK_565 = 0x0000;

	// A single-level texture of `format` made of `bytes`, laid out as
	// texture_level says a level of that size is.
	TextureData texture_of(TextureFormat format, int width, int height,
		const std::vector<unsigned char>& bytes)
	{
		TextureData texture;
		texture.width = width;
		texture.height = height;
		texture.format = format;
		texture.levels.push_back(texture_level(format, width, height, 0));
		texture.pixels = bytes;
		REQUIRE(texture.pixels.size() == texture.levels[0].size);
		return texture;
	}

	// The eight bytes of a colour block: two endpoints, little-endian, then
	// sixteen two-bit indices, texel 0 in the low bits.
	void append_colour(std::vector<unsigned char>& bytes, unsigned int first,
		unsigned int second, const int indices[16])
	{
		bytes.push_back(static_cast<unsigned char>(first & 0xFFu));
		bytes.push_back(static_cast<unsigned char>(first >> 8));
		bytes.push_back(static_cast<unsigned char>(second & 0xFFu));
		bytes.push_back(static_cast<unsigned char>(second >> 8));
		std::uint32_t packed = 0;
		for (int texel = 0; texel < 16; texel++)
		{
			packed |= static_cast<std::uint32_t>(indices[texel]) << (2 * texel);
		}
		for (int byte = 0; byte < 4; byte++)
		{
			bytes.push_back(static_cast<unsigned char>(packed >> (8 * byte)));
		}
	}

	// A colour block every texel of which is `index`.
	void append_solid_colour(std::vector<unsigned char>& bytes,
		unsigned int first, unsigned int second, int index)
	{
		int indices[16];
		for (int texel = 0; texel < 16; texel++)
		{
			indices[texel] = index;
		}
		append_colour(bytes, first, second, indices);
	}

	// The eight bytes of a BC3 alpha block: two endpoints, then sixteen
	// three-bit indices in six bytes, texel 0 in the low bits.
	void append_interpolated_alpha(std::vector<unsigned char>& bytes,
		int first, int second, const int indices[16])
	{
		bytes.push_back(static_cast<unsigned char>(first));
		bytes.push_back(static_cast<unsigned char>(second));
		std::uint64_t packed = 0;
		for (int texel = 0; texel < 16; texel++)
		{
			packed |= static_cast<std::uint64_t>(indices[texel]) << (3 * texel);
		}
		for (int byte = 0; byte < 6; byte++)
		{
			bytes.push_back(static_cast<unsigned char>(packed >> (8 * byte)));
		}
	}

	Rgba at(const TextureData& decoded, size_t level_index, int x, int y)
	{
		REQUIRE(decoded.format == TextureFormat::r8g8b8a8_unorm);
		REQUIRE(level_index < decoded.levels.size());
		const TextureLevel& level = decoded.levels[level_index];
		REQUIRE(x < level.width);
		REQUIRE(y < level.height);
		const size_t byte = level.offset +
			static_cast<size_t>(y) * static_cast<size_t>(level.stride) +
			static_cast<size_t>(x) * 4;
		REQUIRE(byte + 4 <= decoded.pixels.size());
		return Rgba{ decoded.pixels[byte], decoded.pixels[byte + 1],
			decoded.pixels[byte + 2], decoded.pixels[byte + 3] };
	}

	// Texel `index` of a 4x4 block, counted along rows from the top left.
	Rgba texel(const TextureData& decoded, int index)
	{
		return at(decoded, 0, index % 4, index / 4);
	}
}

TEST_CASE("BC1 with the first endpoint above the second decodes four colours")
{
	// Each row is indices 0, 1, 2, 3, so each row holds the whole palette.
	int indices[16];
	for (int i = 0; i < 16; i++)
	{
		indices[i] = i % 4;
	}
	std::vector<unsigned char> bytes;
	append_colour(bytes, RED_565, BLUE_565, indices);

	const TextureData decoded = decode_block_compressed(
		texture_of(TextureFormat::bc1_unorm, 4, 4, bytes));

	CHECK(decoded.width == 4);
	CHECK(decoded.height == 4);
	REQUIRE(decoded.levels.size() == 1);
	CHECK(decoded.levels[0].stride == 16);
	CHECK(decoded.pixels.size() == 64u);

	for (int row = 0; row < 4; row++)
	{
		CHECK(texel(decoded, row * 4 + 0) == Rgba{ 255, 0, 0, 255 });
		CHECK(texel(decoded, row * 4 + 1) == Rgba{ 0, 0, 255, 255 });
		// Two thirds red and one third blue: 170.33 and 85.33, to nearest.
		CHECK(texel(decoded, row * 4 + 2) == Rgba{ 170, 0, 85, 255 });
		CHECK(texel(decoded, row * 4 + 3) == Rgba{ 85, 0, 170, 255 });
	}
}

TEST_CASE("BC1 with the first endpoint not above the second decodes three colours and transparent black")
{
	int indices[16];
	for (int i = 0; i < 16; i++)
	{
		indices[i] = i % 4;
	}

	SUBCASE("first below second")
	{
		std::vector<unsigned char> bytes;
		append_colour(bytes, BLUE_565, RED_565, indices);
		const TextureData decoded = decode_block_compressed(
			texture_of(TextureFormat::bc1_unorm, 4, 4, bytes));

		CHECK(texel(decoded, 0) == Rgba{ 0, 0, 255, 255 });
		CHECK(texel(decoded, 1) == Rgba{ 255, 0, 0, 255 });
		// The midpoint, 127.5, to nearest.
		CHECK(texel(decoded, 2) == Rgba{ 128, 0, 128, 255 });
		// All four channels zero, which is transparent under a
		// premultiplied blend.
		CHECK(texel(decoded, 3) == Rgba{ 0, 0, 0, 0 });
		CHECK(texel(decoded, 15) == Rgba{ 0, 0, 0, 0 });
	}

	SUBCASE("equal endpoints are the three-colour mode too")
	{
		std::vector<unsigned char> bytes;
		append_colour(bytes, WHITE_565, WHITE_565, indices);
		const TextureData decoded = decode_block_compressed(
			texture_of(TextureFormat::bc1_unorm, 4, 4, bytes));

		CHECK(texel(decoded, 0) == Rgba{ 255, 255, 255, 255 });
		CHECK(texel(decoded, 2) == Rgba{ 255, 255, 255, 255 });
		CHECK(texel(decoded, 3) == Rgba{ 0, 0, 0, 0 });
	}
}

TEST_CASE("BC2 alpha is explicit, four bits a texel, and its colour block is always four colours")
{
	// Alphas 0 to 15 in texel order, two a byte, low nibble first.
	std::vector<unsigned char> bytes;
	for (int pair = 0; pair < 8; pair++)
	{
		bytes.push_back(static_cast<unsigned char>(
			(2 * pair) | ((2 * pair + 1) << 4)));
	}
	// First endpoint below second, which in BC1 would make index 3
	// transparent black. In BC2 it is the third of the four colours.
	append_solid_colour(bytes, BLUE_565, RED_565, 3);

	const TextureData decoded = decode_block_compressed(
		texture_of(TextureFormat::bc2_unorm, 4, 4, bytes));

	for (int i = 0; i < 16; i++)
	{
		CAPTURE(i);
		// One third blue and two thirds red, and a nibble times 17, so
		// that 0 is 0 and 15 is 255.
		CHECK(texel(decoded, i) == Rgba{ 170, 0, 85, i * 17 });
	}
}

TEST_CASE("BC3 alpha with the first endpoint above the second interpolates six alphas")
{
	int alpha_indices[16];
	for (int i = 0; i < 16; i++)
	{
		alpha_indices[i] = i % 8;
	}
	std::vector<unsigned char> bytes;
	append_interpolated_alpha(bytes, 255, 0, alpha_indices);
	append_solid_colour(bytes, BLUE_565, RED_565, 3);

	const TextureData decoded = decode_block_compressed(
		texture_of(TextureFormat::bc3_unorm, 4, 4, bytes));

	// 255 and 0, then 6/7 to 1/7 of 255 to nearest: 218.57, 182.14, 145.71,
	// 109.29, 72.86, 36.43.
	const int EXPECTED[8] = { 255, 0, 219, 182, 146, 109, 73, 36 };
	for (int i = 0; i < 16; i++)
	{
		CAPTURE(i);
		// And the colour block is BC2's: always four colours.
		CHECK(texel(decoded, i) == Rgba{ 170, 0, 85, EXPECTED[i % 8] });
	}
}

TEST_CASE("BC3 alpha with the first endpoint not above the second interpolates four, and has 0 and 255")
{
	int alpha_indices[16];
	for (int i = 0; i < 16; i++)
	{
		alpha_indices[i] = 7 - (i % 8);
	}
	std::vector<unsigned char> bytes;
	append_interpolated_alpha(bytes, 0, 255, alpha_indices);
	append_solid_colour(bytes, GREEN_565, BLACK_565, 0);

	const TextureData decoded = decode_block_compressed(
		texture_of(TextureFormat::bc3_unorm, 4, 4, bytes));

	// Indices 0 and 1 are the endpoints, 2 to 5 are 1/5 to 4/5 of the way
	// from the first to the second, and 6 and 7 are 0 and 255 whatever the
	// endpoints are.
	const int EXPECTED[8] = { 0, 255, 51, 102, 153, 204, 0, 255 };
	for (int i = 0; i < 16; i++)
	{
		CAPTURE(i);
		CHECK(texel(decoded, i) == Rgba{ 0, 255, 0, EXPECTED[7 - (i % 8)] });
	}

	SUBCASE("equal endpoints are this mode too")
	{
		std::vector<unsigned char> equal;
		append_interpolated_alpha(equal, 100, 100, alpha_indices);
		append_solid_colour(equal, GREEN_565, BLACK_565, 0);
		const TextureData same = decode_block_compressed(
			texture_of(TextureFormat::bc3_unorm, 4, 4, equal));
		// Texel 0 is index 7 and texel 1 index 6.
		CHECK(texel(same, 0).alpha == 255);
		CHECK(texel(same, 1).alpha == 0);
		CHECK(texel(same, 2).alpha == 100);
	}
}

TEST_CASE("a level whose size is not a multiple of four keeps only the texels inside it")
{
	// 5x3 is two blocks by one. The left block is red and the right blue,
	// and of the right block only its first column is inside the image.
	std::vector<unsigned char> bytes;
	append_solid_colour(bytes, RED_565, BLACK_565, 0);
	append_solid_colour(bytes, BLUE_565, BLACK_565, 0);

	const TextureData decoded = decode_block_compressed(
		texture_of(TextureFormat::bc1_unorm, 5, 3, bytes));

	CHECK(decoded.width == 5);
	CHECK(decoded.height == 3);
	REQUIRE(decoded.levels.size() == 1);
	CHECK(decoded.levels[0].width == 5);
	CHECK(decoded.levels[0].height == 3);
	CHECK(decoded.levels[0].stride == 20);
	CHECK(decoded.levels[0].rows == 3);
	CHECK(decoded.pixels.size() == 60u);

	for (int y = 0; y < 3; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			CHECK(at(decoded, 0, x, y) == Rgba{ 255, 0, 0, 255 });
		}
		CHECK(at(decoded, 0, 4, y) == Rgba{ 0, 0, 255, 255 });
	}
}

TEST_CASE("every level of a mip chain is decoded, and packed one after another")
{
	// 8x8, 4x4, 2x2 and 1x1 - four blocks, then one each - in four colours,
	// so that a level read from another's offset shows.
	TextureData texture;
	texture.width = 8;
	texture.height = 8;
	texture.format = TextureFormat::bc1_unorm;
	const unsigned int COLOURS[4] = { RED_565, GREEN_565, BLUE_565, WHITE_565 };
	int size = 8;
	for (int level = 0; level < 4; level++)
	{
		texture.levels.push_back(texture_level(texture.format, size, size,
			texture.pixels.size()));
		const int blocks = ((size + 3) / 4) * ((size + 3) / 4);
		for (int block = 0; block < blocks; block++)
		{
			append_solid_colour(texture.pixels, COLOURS[level], BLACK_565, 0);
		}
		size /= 2;
	}

	const TextureData decoded = decode_block_compressed(texture);

	REQUIRE(decoded.levels.size() == 4);
	const int SIZES[4] = { 8, 4, 2, 1 };
	const size_t OFFSETS[4] = { 0, 256, 320, 336 };
	for (size_t level = 0; level < 4; level++)
	{
		CAPTURE(level);
		CHECK(decoded.levels[level].width == SIZES[level]);
		CHECK(decoded.levels[level].height == SIZES[level]);
		CHECK(decoded.levels[level].offset == OFFSETS[level]);
		CHECK(decoded.levels[level].size ==
			static_cast<size_t>(SIZES[level] * SIZES[level] * 4));
	}
	CHECK(decoded.pixels.size() == 340u);

	CHECK(at(decoded, 0, 0, 0) == Rgba{ 255, 0, 0, 255 });
	CHECK(at(decoded, 0, 7, 7) == Rgba{ 255, 0, 0, 255 });
	CHECK(at(decoded, 1, 3, 3) == Rgba{ 0, 255, 0, 255 });
	CHECK(at(decoded, 2, 1, 1) == Rgba{ 0, 0, 255, 255 });
	CHECK(at(decoded, 3, 0, 0) == Rgba{ 255, 255, 255, 255 });
}

TEST_CASE("the shipped font atlas decodes to one pixel per texel, every alpha a whole step")
{
	// The real file the pixel tests draw text from, so the decoder is held
	// to what the content is and not only to blocks built for it.
	const TextureData atlas = labrador::read_sprite_font_file(
		"./content/courier_new_bold_16.spritefont").atlas;
	REQUIRE(atlas.format == TextureFormat::bc2_unorm);

	const TextureData decoded = decode_block_compressed(atlas);

	CHECK(decoded.width == atlas.width);
	CHECK(decoded.height == atlas.height);
	CHECK(decoded.pixels.size() ==
		static_cast<size_t>(atlas.width) * static_cast<size_t>(atlas.height) * 4);
	bool every_alpha_a_step = true;
	for (size_t byte = 3; byte < decoded.pixels.size(); byte += 4)
	{
		every_alpha_a_step = every_alpha_a_step && decoded.pixels[byte] % 17 == 0;
	}
	CHECK(every_alpha_a_step);
}

TEST_CASE("a texture that is not block compressed, or whose levels do not fit, is refused")
{
	std::vector<unsigned char> bytes;
	append_solid_colour(bytes, RED_565, BLACK_565, 0);

	SUBCASE("an uncompressed format")
	{
		TextureData texture;
		texture.width = 1;
		texture.height = 1;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 1, 1, 0));
		texture.pixels.assign(4, 255);
		CHECK_THROWS_AS(decode_block_compressed(texture), std::runtime_error);
	}

	SUBCASE("no levels")
	{
		TextureData texture = texture_of(TextureFormat::bc1_unorm, 4, 4, bytes);
		texture.levels.clear();
		CHECK_THROWS_AS(decode_block_compressed(texture), std::runtime_error);
	}

	SUBCASE("a level that runs past the bytes")
	{
		TextureData texture = texture_of(TextureFormat::bc1_unorm, 4, 4, bytes);
		texture.pixels.pop_back();
		CHECK_THROWS_AS(decode_block_compressed(texture), std::runtime_error);
	}

	SUBCASE("a level laid out for another format")
	{
		TextureData texture = texture_of(TextureFormat::bc1_unorm, 4, 4, bytes);
		texture.format = TextureFormat::bc3_unorm;
		CHECK_THROWS_AS(decode_block_compressed(texture), std::runtime_error);
	}
}

#include <doctest/doctest.h>

#include "engine/render/dds_file.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "tests/scratch_file.h"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <string>
#include <vector>

// Reading a .dds, with no device.
//
// READING IS SEPARATE FROM MAKING THE TEXTURE, so what the reader thinks a file
// says is assertable without drawing the result, and the device half is three
// lines.
//
// AGAINST THE REAL FILE FOR THE HAPPY PATH, and against files built here for
// the unhappy ones. tests/render/content/quad.dds is the four-colour texture
// every sprite case in RenderPixelTests draws, so what this file says about it
// is what those cases are actually looking at.

namespace
{
	using labrador::TextureData;
	using labrador::TextureFormat;
	using labrador::TextureLevel;
	using labrador::read_dds_file;
	using labrador::texture_level;
	using labrador_tests::ScratchFile;

	const char* quad_path = "./content/quad.dds";

	std::vector<unsigned char> quad_bytes()
	{
		std::ifstream file(quad_path, std::ios::binary | std::ios::ate);
		REQUIRE(file.good());
		const std::streamoff size = file.tellg();
		std::vector<unsigned char> bytes(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(bytes.data()), size);
		return bytes;
	}

	// A little-endian DWORD, into a header being edited in place.
	void poke(std::vector<unsigned char>& bytes, size_t offset, uint32_t value)
	{
		for (size_t i = 0; i < 4; i++)
		{
			bytes[offset + i] =
				static_cast<unsigned char>((value >> (8 * i)) & 0xFFu);
		}
	}

	// Byte offsets into a .dds, which are fixed for every file there is: a
	// four-byte magic, then a 124-byte header whose fields never move. Written
	// as the numbers they are rather than as arithmetic over field counts,
	// because the arithmetic is exactly what one of these was off by one on.
	const size_t header_size_at = 4;
	const size_t height_at = 12;
	const size_t width_at = 16;
	const size_t depth_at = 24;
	const size_t mip_count = 28;
	const size_t pixel_format_size_at = 76;
	const size_t pixel_flags = 80;
	const size_t four_cc_at = 84;
	const size_t alpha_mask = 104;
	const size_t caps2 = 112;

	// The fourCC flag; with it set the masks and bit count are not read.
	const uint32_t four_cc_flag = 0x4;

	uint32_t four_cc_of(const char (&code)[5])
	{
		return static_cast<uint32_t>(static_cast<unsigned char>(code[0])) |
			(static_cast<uint32_t>(static_cast<unsigned char>(code[1])) << 8) |
			(static_cast<uint32_t>(static_cast<unsigned char>(code[2])) << 16) |
			(static_cast<uint32_t>(static_cast<unsigned char>(code[3])) << 24);
	}

	// The message of the refusal `bytes` earn, written to a file named
	// `name`. Every refusal must name the file, and that is checked here, so
	// each case only checks the words that say which refusal it was.
	std::string refusal_of(const std::vector<unsigned char>& bytes,
		const std::string& name)
	{
		const ScratchFile file(name, bytes);
		try
		{
			(void)read_dds_file(file.path());
		}
		catch (const std::runtime_error& error)
		{
			const std::string message = error.what();
			CHECK(message.find(file.path()) != std::string::npos);
			return message;
		}
		FAIL("the file was accepted");
		return {};
	}
}

TEST_CASE("the test texture reads back as the four-colour image it is")
{
	const TextureData quad = read_dds_file(quad_path);

	CHECK(quad.width == 2);
	CHECK(quad.height == 2);

	// BGRA AND NOT RGBA, which is the assertion the channel masks exist for
	// and the one nothing would notice going wrong until red and blue swapped
	// on screen. The back buffer is BGRA too, and read_back_buffer converts on
	// the way out - so a texture read as the wrong one of the two would come
	// back through that conversion looking almost right.
	CHECK(quad.format == TextureFormat::b8g8r8a8_unorm);

	REQUIRE(quad.levels.size() == 1);
	CHECK(quad.levels[0].stride == 8);
	CHECK(quad.levels[0].rows == 2);
	CHECK(quad.pixels.size() == 16u);
}

TEST_CASE("a mip chain is walked, and a block-compressed one rounds up")
{
	// NO FILE IN EITHER CLIENT HAS MIPS, so this is asserted on the arithmetic
	// rather than on a file - but the arithmetic is what a .dds expects a
	// reader to know and states nowhere, and it is the half of reading one that
	// is easiest to get quietly wrong. A level's bytes follow the one before
	// it with no offset table, so a single wrong size shifts every level after
	// it and the texture is noise from that point down.
	CHECK(texture_level(TextureFormat::b8g8r8a8_unorm, 8, 4, 0).stride == 32);
	CHECK(texture_level(TextureFormat::b8g8r8a8_unorm, 8, 4, 0).rows == 4);

	// A 4x4 block is 16 bytes for BC2 and BC3 and 8 for BC1, so an 8x8 BC3
	// level is two blocks by two.
	CHECK(texture_level(TextureFormat::bc3_unorm, 8, 8, 0).size == 64u);
	CHECK(texture_level(TextureFormat::bc1_unorm, 8, 8, 0).size == 32u);

	// THE TWO CASES AT THE BOTTOM OF A CHAIN. A 6x6 level is two blocks by two
	// and not one and a half, and a 1x1 level is still a whole block - so a
	// reader that divided would stop short and leave the last level's bytes
	// unread.
	CHECK(texture_level(TextureFormat::bc3_unorm, 6, 6, 0).size == 64u);
	CHECK(texture_level(TextureFormat::bc3_unorm, 1, 1, 0).size == 16u);

	CHECK(labrador::is_block_compressed(TextureFormat::bc2_unorm));
	CHECK_FALSE(labrador::is_block_compressed(TextureFormat::b8g8r8a8_unorm));
}

TEST_CASE("a file that is not a texture this engine draws says which way")
{
	SUBCASE("nothing there")
	{
		CHECK_THROWS_AS(read_dds_file("./content/no_such.dds"),
			std::out_of_range);
	}

	SUBCASE("there, and not a .dds")
	{
		const std::vector<unsigned char> junk(200, 0x41);
		const ScratchFile file("not_a_texture.dds", junk);

		// A different throw from the one above, and the reason is the one the
		// font reader has: "failed to load texture (0x80070002)" was every
		// failure's message, and eight hex digits are not an answer to which
		// of the two it was.
		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}

	SUBCASE("a cube map")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, caps2, 0x200);
		const ScratchFile file("cube.dds", bytes);

		// Six faces, and this engine has somewhere to put one. Handing back
		// the first face would draw something, which is worse.
		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}

	SUBCASE("a format with no alpha channel")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		// Clear DDPF_ALPHAPIXELS, leaving DDPF_RGB and a zero alpha mask -
		// which is what an image exported without transparency looks like, and
		// is by far the likeliest way a real file lands here.
		poke(bytes, pixel_flags, 0x40);
		poke(bytes, alpha_mask, 0);
		const ScratchFile file("opaque.dds", bytes);

		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}

	SUBCASE("a file that claims more mips than it carries")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		// Two, which a 2x2 texture can have - 2x2 and 1x1 - and which this
		// file does not carry the twenty bytes for.
		poke(bytes, mip_count, 2);
		const ScratchFile file("too_many_mips.dds", bytes);

		// The levels are back to back with no table, so a count that lies is a
		// read past the end of the file - which is what a bounds-checked reader
		// is for, and what mapping a struct over the buffer would not catch.
		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}

	SUBCASE("a file that claims more mips than can exist")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, mip_count, 70000);
		const ScratchFile file("impossible_mips.dds", bytes);

		// REFUSED ON THE COUNT AND NOT ON THE BYTES, which is a different
		// check from the one above and is the reason it exists. A chain is the
		// larger side halved until it reaches one, so a 2x2 texture has two
		// levels and cannot have a third; the walk below the check would
		// happily build seventy thousand of them, all 1x1, because halving one
		// leaves one. A header field deciding how much a process allocates -
		// and how many subresources every backend downstream is then asked
		// about - is the shape worth refusing by name.
		//
		// SEVENTY THOUSAND SPECIFICALLY, because the D3D12 backend narrowed
		// this count to UINT16 for the resource description it builds: a value
		// that fits UINT16 fails resource creation and hits that backend's
		// named throw, and one that does not would be TRUNCATED and succeed on
		// a number no file ever said. Both walls stand, and this is the first
		// of them - the one every backend inherits.
		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}
}

TEST_CASE("texture layouts reject stride and accumulated offset overflow")
{
	const int limit = (std::numeric_limits<int>::max)();
	CHECK(texture_level(TextureFormat::r8g8b8a8_unorm, limit / 4, 1, 0).stride == (limit / 4) * 4);
	CHECK_THROWS_AS(texture_level(TextureFormat::r8g8b8a8_unorm, limit / 4 + 1, 1, 0), std::runtime_error);
	CHECK(texture_level(TextureFormat::bc1_unorm, (limit / 8) * 4, 1, 0).stride == (limit / 8) * 8);
	CHECK_THROWS_AS(texture_level(TextureFormat::bc1_unorm, (limit / 8) * 4 + 1, 1, 0), std::runtime_error);
	// One block column, as tall as int allows: exactly 4 GiB. That fits a
	// 64-bit size_t and is one byte past a 32-bit one, which is WebAssembly's,
	// so the same wall answers both ways and each is the right answer.
	if constexpr (sizeof(size_t) > 4)
	{
		CHECK(texture_level(TextureFormat::bc1_unorm, 1, limit, 0).rows == limit / 4 + 1);
	}
	else
	{
		CHECK_THROWS_AS(texture_level(TextureFormat::bc1_unorm, 1, limit, 0), std::runtime_error);
	}
	CHECK_THROWS_AS(texture_level(TextureFormat::bc3_unorm, limit, limit, 0), std::runtime_error);
	CHECK_THROWS_AS(texture_level(TextureFormat::r8g8b8a8_unorm, 1, 1,
		(std::numeric_limits<size_t>::max)() - 3), std::runtime_error);
}

TEST_CASE("texture layouts refuse a format outside TextureFormat by name")
{
	// What a TextureFormat added without a layout, or a corrupt value, would
	// reach. It once laid out as four bytes a pixel with no word said.
	const TextureFormat unknown = static_cast<TextureFormat>(99);
	CHECK_FALSE(labrador::is_block_compressed(unknown));

	std::string message;
	try
	{
		const TextureLevel level = texture_level(unknown, 4, 4, 0, "probe.dds");
		FAIL_CHECK("an unknown format was laid out at stride " << level.stride);
	}
	catch (const std::runtime_error& error)
	{
		message = error.what();
	}
	CHECK(message.find("probe.dds") != std::string::npos);
	CHECK(message.find("TextureFormat 99") != std::string::npos);

	// Every listed format still lays out.
	CHECK(texture_level(TextureFormat::b4g4r4a4_unorm, 4, 4, 0).stride == 8);
	CHECK(texture_level(TextureFormat::b8g8r8a8_unorm, 4, 4, 0).stride == 16);
	CHECK(texture_level(TextureFormat::bc2_unorm, 4, 4, 0).size == 16);
	CHECK(labrador::is_block_compressed(TextureFormat::bc2_unorm));
	CHECK_FALSE(labrador::is_block_compressed(TextureFormat::r8g8b8a8_unorm));
}

TEST_CASE("DDS rejects malformed large dimensions before payload acceptance")
{
	for (unsigned int width : { 0x20000000u, 0x40000001u, 0x7fffffffu, 0x80000000u })
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, 12, 1);
		poke(bytes, 16, width);
		bytes.resize(132);
		const ScratchFile file("overflow_width.dds", bytes);
		try
		{
			(void)read_dds_file(file.path());
			FAIL("malformed DDS was accepted");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find(file.path()) != std::string::npos);
		}
	}
}

TEST_CASE("each named refusal of a .dds header names the file and its reason")
{
	SUBCASE("the DX10 extended header")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, pixel_flags, four_cc_flag);
		poke(bytes, four_cc_at, four_cc_of("DX10"));

		CHECK(refusal_of(bytes, "dx10.dds").find("DX10 extended header") !=
			std::string::npos);
	}

	SUBCASE("a fourCC this engine does not read")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, pixel_flags, four_cc_flag);
		poke(bytes, four_cc_at, four_cc_of("ATI2"));

		const std::string message = refusal_of(bytes, "ati2.dds");
		CHECK(message.find("\"ATI2\"") != std::string::npos);
		CHECK(message.find("does not read") != std::string::npos);
	}

	SUBCASE("a volume texture by its caps")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, caps2, 0x200000);

		CHECK(refusal_of(bytes, "volume_caps.dds").find("volume texture") !=
			std::string::npos);
	}

	SUBCASE("a volume texture by its depth")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, depth_at, 4);

		const std::string message = refusal_of(bytes, "volume_depth.dds");
		CHECK(message.find("volume texture") != std::string::npos);
		CHECK(message.find("4 slices") != std::string::npos);
	}

	SUBCASE("a header size other than 124")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, header_size_at, 120);

		CHECK(refusal_of(bytes, "header_size.dds").find("120 byte header") !=
			std::string::npos);
	}

	SUBCASE("a pixel format size other than 32")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, pixel_format_size_at, 28);

		CHECK(refusal_of(bytes, "pixel_format_size.dds").find(
			"28 byte pixel format") != std::string::npos);
	}

	SUBCASE("a zero width")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, width_at, 0);

		CHECK(refusal_of(bytes, "zero_width.dds").find("0x2") !=
			std::string::npos);
	}

	SUBCASE("a zero height")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, height_at, 0);

		CHECK(refusal_of(bytes, "zero_height.dds").find("2x0") !=
			std::string::npos);
	}

	SUBCASE("a file that ends inside its header")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		bytes.resize(60);

		CHECK(refusal_of(bytes, "truncated_header.dds").find("ends after 60 bytes") !=
			std::string::npos);
	}
}

TEST_CASE("DXT2 and DXT4 read as BC2 and BC3, as DXT3 and DXT5 do")
{
	// The quad is 2x2, so its sixteen bytes of pixels are exactly one 4x4
	// block of BC2 or BC3: relabelling the header is a whole valid file.
	struct Mapping
	{
		const char* name;
		char code[5];
		TextureFormat format;
	};
	const Mapping mappings[] = {
		{ "dxt2.dds", "DXT2", TextureFormat::bc2_unorm },
		{ "dxt3.dds", "DXT3", TextureFormat::bc2_unorm },
		{ "dxt4.dds", "DXT4", TextureFormat::bc3_unorm },
		{ "dxt5.dds", "DXT5", TextureFormat::bc3_unorm },
	};
	for (const Mapping& mapping : mappings)
	{
		CAPTURE(mapping.name);
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, pixel_flags, four_cc_flag);
		poke(bytes, four_cc_at, four_cc_of(mapping.code));
		const ScratchFile file(mapping.name, bytes);

		const TextureData texture = read_dds_file(file.path());
		CHECK(texture.format == mapping.format);
		REQUIRE(texture.levels.size() == 1);
		CHECK(texture.levels[0].size == 16u);
	}
}

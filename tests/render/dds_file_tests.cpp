#include <doctest/doctest.h>

#include "engine/render/dds_file.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Reading a .dds, with no device.
//
// WHAT THIS REPLACED HAS NO TESTS ANYWHERE, and could not have any: it was one
// call into DirectXTK that read the file and made the texture together, so the
// only way to check what it thought a file said was to draw the result. The
// split makes the first half assertable and the second half three lines.
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

	const char* QUAD_PATH = "./content/quad.dds";

	class ScratchFile
	{
	public:
		ScratchFile(const std::string& name,
			const std::vector<unsigned char>& bytes)
			: path_(name)
		{
			std::ofstream file(this->path_, std::ios::binary);
			REQUIRE(file.good());
			if (!bytes.empty())
			{
				file.write(reinterpret_cast<const char*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
			}
		}

		~ScratchFile() { std::remove(this->path_.c_str()); }

		ScratchFile(const ScratchFile&) = delete;
		ScratchFile& operator=(const ScratchFile&) = delete;

		const std::string& path() const { return this->path_; }

	private:
		std::string path_;
	};

	std::vector<unsigned char> quad_bytes()
	{
		std::ifstream file(QUAD_PATH, std::ios::binary | std::ios::ate);
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
	const size_t MIP_COUNT = 28;
	const size_t PIXEL_FLAGS = 80;
	const size_t ALPHA_MASK = 104;
	const size_t CAPS2 = 112;
}

TEST_CASE("the test texture reads back as the four-colour image it is")
{
	const TextureData quad = read_dds_file(QUAD_PATH);

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
		poke(bytes, CAPS2, 0x200);
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
		poke(bytes, PIXEL_FLAGS, 0x40);
		poke(bytes, ALPHA_MASK, 0);
		const ScratchFile file("opaque.dds", bytes);

		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}

	SUBCASE("a file that claims more mips than it carries")
	{
		std::vector<unsigned char> bytes = quad_bytes();
		poke(bytes, MIP_COUNT, 4);
		const ScratchFile file("too_many_mips.dds", bytes);

		// The levels are back to back with no table, so a count that lies is a
		// read past the end of the file - which is what a bounds-checked reader
		// is for, and what mapping a struct over the buffer would not catch.
		CHECK_THROWS_AS(read_dds_file(file.path()), std::runtime_error);
	}
}

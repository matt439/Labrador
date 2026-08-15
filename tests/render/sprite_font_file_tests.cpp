#include <doctest/doctest.h>

#include "engine/render/sprite_font_file.h"
#include "engine/render/font.h"
#include "engine/render/texture_format.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Reading a .spritefont, with no device.
//
// AGAINST THE REAL FILE, not a fixture built to be read. The one in
// tests/render/content/ is the font both samples ship and is exactly what
// MakeSpriteFont writes when nobody chooses anything: 95 glyphs, U+0020 to
// U+007E, no default character, and a BC2 atlas. Every assertion below that
// names a number is naming a property of that file, and a fixture would have
// let the parser and the fixture agree with each other and with nothing else.
//
// THE ERRORS MATTER AS MUCH AS THE GLYPHS. Loading a font used to report every
// possible failure as "not found at <path>", so a truncated file, a file from
// a newer tool and a genuinely missing one were one message. They are three
// now, and a build step that half-writes a font is the reason to check.

namespace
{
	using labrador::Glyph;
	using labrador::SpriteFontFile;
	using labrador::TextureFormat;
	using labrador::read_sprite_font_file;

	const char* FONT_PATH = "./content/courier_new_bold_16.spritefont";

	// A file in the working directory, removed when the test leaves.
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

	std::vector<unsigned char> whole_font()
	{
		std::ifstream file(FONT_PATH, std::ios::binary | std::ios::ate);
		REQUIRE(file.good());
		const std::streamoff size = file.tellg();
		std::vector<unsigned char> bytes(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(bytes.data()), size);
		return bytes;
	}
}

TEST_CASE("the sample font reads back as the table MakeSpriteFont wrote")
{
	const SpriteFontFile font = read_sprite_font_file(FONT_PATH);

	// The default region and nothing else, which is the fact the stand-in
	// glyph exists because of: 95 characters is U+0020 to U+007E, so every
	// curly apostrophe and en-dash a text editor inserts is outside it.
	CHECK(font.glyphs.size() == 95);
	CHECK(font.glyphs.front().character == U' ');
	CHECK(font.glyphs.back().character == U'~');

	// NOBODY CHOSE A DEFAULT CHARACTER, which is what MakeSpriteFont writes
	// when it is not told to. The resource factory is what turns that into a
	// question mark, and it can only do so because this is reported rather
	// than swallowed.
	CHECK(font.stand_in == 0);

	CHECK(font.line_spacing == doctest::Approx(24.1667f));

	// A RECT in the file, a position and a size out here. Courier is
	// monospaced, so every character steps the pen by the same amount - and
	// that amount is bearing plus width plus adjustment, which is the sum that
	// would not come out round if any of the three were read into the wrong
	// field.
	for (const Glyph& glyph : font.glyphs)
	{
		CHECK(glyph.subrect.width > 0);
		CHECK(glyph.subrect.height > 0);
		CHECK(glyph.x_offset + static_cast<float>(glyph.subrect.width) +
			glyph.x_advance == doctest::Approx(13.0f));
	}
}

TEST_CASE("the atlas comes out as bytes and a format, not as a texture")
{
	const SpriteFontFile font = read_sprite_font_file(FONT_PATH);

	CHECK(font.atlas.width == 128);
	CHECK(font.atlas.height == 144);

	// BC2, WHICH IS THE ONE TO KNOW ABOUT BEFORE A SECOND BACKEND. Desktop GL
	// reads it through an extension and GLES 3.0 cannot read it at all, so a
	// backend that skipped it would have every glyph in both clients and no
	// way to upload one.
	CHECK(font.atlas.format == TextureFormat::bc2_unorm);

	// One level. The format has no way to carry a chain, which is why this is
	// an assertion rather than an omission.
	REQUIRE(font.atlas.levels.size() == 1);

	// Four-by-four blocks of sixteen bytes: 128 pixels is 32 blocks is 512
	// bytes a row, and 144 pixels is 36 rows OF BLOCKS. Getting this wrong the
	// obvious way gives 144 rows of 512 and asks the device for four times the
	// data there is.
	CHECK(font.atlas.levels[0].stride == 512);
	CHECK(font.atlas.levels[0].rows == 36);
	CHECK(font.atlas.levels[0].offset == 0);
	CHECK(font.atlas.pixels.size() == 512u * 36u);
}

TEST_CASE("a file that is not a font says which way it is not one")
{
	SUBCASE("nothing there")
	{
		CHECK_THROWS_AS(read_sprite_font_file("./content/no_such.spritefont"),
			std::out_of_range);
	}

	SUBCASE("there, and not a MakeSpriteFont binary")
	{
		const std::vector<unsigned char> junk(64, 0x41);
		const ScratchFile file("not_a_font.spritefont", junk);

		// A DIFFERENT THROW FROM THE ONE ABOVE, and that is the point. These
		// were the same exception with the same message, so a font that was
		// present and wrong reported itself as absent - and the first thing
		// anybody does with "not found at <path>" is check that the path
		// exists, which it did.
		CHECK_THROWS_AS(read_sprite_font_file(file.path()),
			std::runtime_error);
	}

	SUBCASE("a real font with its end cut off")
	{
		std::vector<unsigned char> bytes = whole_font();
		bytes.resize(bytes.size() / 2);
		const ScratchFile file("half_a_font.spritefont", bytes);

		// The atlas is the last thing in the file and by far the largest, so a
		// half-written font parses perfectly until it asks for pixels. Reading
		// a struct straight off the buffer would have read past the end of it.
		CHECK_THROWS_AS(read_sprite_font_file(file.path()),
			std::runtime_error);
	}
}

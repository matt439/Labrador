#include <doctest/doctest.h>

#include "engine/render/sprite_font_file.h"
#include "engine/render/font.h"
#include "engine/render/texture_format.h"
#include "tests/scratch_file.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Reading a .spritefont, with no device.
//
// AGAINST THE REAL FILE, not a fixture built to be read. The one in
// tests/render/content/ is the font minimal and LineSweeper ship and is what
// MakeSpriteFont writes when nobody chooses anything: 95 glyphs, U+0020 to
// U+007E, no default character, and a BC2 atlas. Every assertion below that
// names a number is naming a property of that file, and a fixture would have
// let the parser and the fixture agree with each other and with nothing else.
//
// THE ERRORS MATTER AS MUCH AS THE GLYPHS. One message for every failure -
// "not found at <path>" - makes a truncated file, a file from a newer tool and
// a genuinely missing one indistinguishable. They are three here, and a build
// step that half-writes a font is the reason to check.

namespace
{
	using labrador::Glyph;
	using labrador::SpriteFontFile;
	using labrador::TextureFormat;
	using labrador::read_sprite_font_file;
	using labrador_tests::ScratchFile;

	const char* font_path = "./content/jetbrains_mono_bold_16.spritefont";

	std::vector<unsigned char> whole_font()
	{
		std::ifstream file(font_path, std::ios::binary | std::ios::ate);
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
	const SpriteFontFile font = read_sprite_font_file(font_path);

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

	CHECK(font.line_spacing == doctest::Approx(28.16f));

	// A RECT in the file, a position and a size out here. The font is
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
	const SpriteFontFile font = read_sprite_font_file(font_path);

	CHECK(font.atlas.width == 128);
	CHECK(font.atlas.height == 168);

	// BC2, WHICH IS THE ONE TO KNOW ABOUT BEFORE A SECOND BACKEND. Desktop GL
	// reads it through an extension, and GLES 3.0 - WebGL2 included - only
	// through an extension it does not guarantee, so a backend that skipped it
	// would have every glyph in both clients and no way to upload one.
	CHECK(font.atlas.format == TextureFormat::bc2_unorm);

	// One level. The format has no way to carry a chain, which is why this is
	// an assertion rather than an omission.
	REQUIRE(font.atlas.levels.size() == 1);

	// Four-by-four blocks of sixteen bytes: 128 pixels is 32 blocks is 512
	// bytes a row, and 168 pixels is 42 rows OF BLOCKS. Getting this wrong the
	// obvious way gives 168 rows of 512 and asks the device for four times the
	// data there is.
	CHECK(font.atlas.levels[0].stride == 512);
	CHECK(font.atlas.levels[0].rows == 42);
	CHECK(font.atlas.levels[0].offset == 0);
	CHECK(font.atlas.pixels.size() == 512u * 42u);
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

TEST_CASE("spritefont rejects overflowed atlas layouts with the source name")
{
	for (unsigned int width : { 0x20000000u, 0x40000001u, 0x7fffffffu, 0x80000000u })
	{
		std::vector<unsigned char> bytes{ 'D', 'X', 'T', 'K', 'f', 'o', 'n', 't' };
		const auto append = [&bytes](unsigned int value)
		{
			for (int shift = 0; shift < 32; shift += 8)
				bytes.push_back(static_cast<unsigned char>(value >> shift));
		};
		append(0); // glyph count
		append(0x3f800000); // line spacing = 1
		append(0); // fallback character
		append(width);
		append(1); // height
		append(28); // RGBA
		append(4); // the stride width * 4 wraps to in 32 bits
		append(1); // rows
		append(0xffffffff); // only four payload bytes
		const ScratchFile file("overflow_atlas.spritefont", bytes);
		try
		{
			(void)read_sprite_font_file(file.path());
			FAIL("malformed atlas was accepted");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find(file.path()) != std::string::npos);
		}
	}
}

TEST_CASE("spritefont rejects a glyph count larger than the file, naming it")
{
	// 2^27 glyphs of 32 bytes is exactly 2^32 bytes, which a 32-bit size_t -
	// the wasm build's - wraps to zero. The others are the next count past it
	// and the largest the field can hold. Each must be refused by the count
	// check, which names the file and the count, rather than by whatever
	// tries to allocate for it afterwards.
	for (unsigned int glyph_count : { 0x08000000u, 0x08000001u, 0xffffffffu })
	{
		std::vector<unsigned char> bytes{ 'D', 'X', 'T', 'K', 'f', 'o', 'n', 't' };
		const auto append = [&bytes](unsigned int value)
		{
			for (int shift = 0; shift < 32; shift += 8)
				bytes.push_back(static_cast<unsigned char>(value >> shift));
		};
		append(glyph_count);
		for (int i = 0; i < 8; i++)
		{
			append(0); // one glyph's worth of bytes, and no more
		}
		const ScratchFile file("huge_glyph_count.spritefont", bytes);
		try
		{
			(void)read_sprite_font_file(file.path());
			FAIL("a glyph count larger than the file was accepted");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message = error.what();
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find(std::to_string(glyph_count) + " glyphs") !=
				std::string::npos);
		}
	}
}

namespace
{
	struct GlyphRectangleEdges
	{
		int32_t left;
		int32_t top;
		int32_t right;
		int32_t bottom;
	};

	std::vector<unsigned char> one_glyph_font(const GlyphRectangleEdges& edges)
	{
		std::vector<unsigned char> bytes{ 'D', 'X', 'T', 'K', 'f', 'o', 'n', 't' };
		const auto append = [&bytes](uint32_t value)
		{
			for (int shift = 0; shift < 32; shift += 8)
			{
				bytes.push_back(static_cast<unsigned char>(value >> shift));
			}
		};
		append(1); // glyph count
		append(U'A');
		append(static_cast<uint32_t>(edges.left));
		append(static_cast<uint32_t>(edges.top));
		append(static_cast<uint32_t>(edges.right));
		append(static_cast<uint32_t>(edges.bottom));
		append(0); // x offset
		append(0); // y offset
		append(0); // advance adjustment
		append(0x3f800000u); // line spacing = 1
		append(0); // default character
		append(1); // atlas width
		append(1); // atlas height
		append(28); // RGBA
		append(4); // stride
		append(1); // rows
		append(0xffffffffu); // one complete white pixel
		return bytes;
	}
}

TEST_CASE("spritefont refuses unrepresentable glyph rectangle extents")
{
	const int32_t minimum = (std::numeric_limits<int32_t>::min)();
	const int32_t maximum = (std::numeric_limits<int32_t>::max)();
	const GlyphRectangleEdges cases[] = {
		{ minimum, 0, maximum, 1 },
		{ maximum, 0, minimum, 1 },
		{ 0, minimum, 1, maximum },
		{ 0, maximum, 1, minimum }
	};
	for (const GlyphRectangleEdges& edges : cases)
	{
		const std::vector<unsigned char> bytes = one_glyph_font(edges);
		REQUIRE(bytes.size() == 76);
		const ScratchFile file("glyph_extent_overflow.spritefont", bytes);
		try
		{
			static_cast<void>(read_sprite_font_file(file.path()));
			FAIL("unrepresentable glyph rectangle was accepted");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message = error.what();
			CHECK(message.find(file.path()) != std::string::npos);
			CHECK(message.find("glyph 0") != std::string::npos);
			CHECK(message.find("rectangle extent") != std::string::npos);
			CHECK(message.find("int range") != std::string::npos);
		}
	}
}

TEST_CASE("spritefont preserves representable signed and zero glyph extents")
{
	struct Control
	{
		GlyphRectangleEdges edges;
		int width;
		int height;
	};
	const int32_t minimum = (std::numeric_limits<int32_t>::min)();
	const int32_t maximum = (std::numeric_limits<int32_t>::max)();
	const Control controls[] = {
		{ { 2, 3, 6, 8 }, 4, 5 },
		{ { 0, 0, 0, 0 }, 0, 0 },
		{ { minimum, maximum, -1, -1 }, maximum, minimum },
		{ { maximum, minimum, -1, -1 }, minimum, maximum }
	};
	for (const Control& control : controls)
	{
		const std::vector<unsigned char> bytes = one_glyph_font(control.edges);
		REQUIRE(bytes.size() == 76);
		const ScratchFile file("glyph_extent_control.spritefont", bytes);
		const SpriteFontFile font = read_sprite_font_file(file.path());
		REQUIRE(font.glyphs.size() == 1);
		const Glyph& glyph = font.glyphs[0];
		CHECK(glyph.character == U'A');
		CHECK(glyph.subrect.x == control.edges.left);
		CHECK(glyph.subrect.y == control.edges.top);
		CHECK(glyph.subrect.width == control.width);
		CHECK(glyph.subrect.height == control.height);
		CHECK(font.line_spacing == 1.0f);
		CHECK(font.atlas.width == 1);
		CHECK(font.atlas.height == 1);
		CHECK(font.atlas.pixels == std::vector<unsigned char>(4, 0xff));
	}
}

namespace
{
	struct FontMetricField
	{
		const char* name;
		std::size_t byte_offset;
		bool glyph_metric;
	};

	const std::array<FontMetricField, 4> font_metric_fields = { {
		{ "x_offset", 32, true },
		{ "y_offset", 36, true },
		{ "x_advance", 40, true },
		{ "line_spacing", 44, false }
	} };

	void overwrite_font_word(std::vector<unsigned char>& bytes,
		std::size_t byte_offset, uint32_t bits)
	{
		REQUIRE(bytes.size() >= 4);
		REQUIRE(byte_offset <= bytes.size() - 4);
		for (std::size_t index = 0; index < 4; ++index)
		{
			bytes[byte_offset + index] =
				static_cast<unsigned char>((bits >> (8 * index)) & 0xffu);
		}
	}
}

TEST_CASE("spritefont refuses non-finite file metrics by field")
{
	const std::array<uint32_t, 3> nonfinite_bits = {
		0x7fc00000u, 0x7f800000u, 0xff800000u
	};
	for (const FontMetricField& field : font_metric_fields)
	{
		for (const uint32_t bits : nonfinite_bits)
		{
			CAPTURE(field.name);
			CAPTURE(bits);
			std::vector<unsigned char> bytes =
				one_glyph_font(GlyphRectangleEdges{ 0, 0, 1, 1 });
			REQUIRE(bytes.size() == 76);
			overwrite_font_word(bytes, field.byte_offset, bits);
			const ScratchFile file("nonfinite-font-metric.spritefont", bytes);
			bool rejected = false;
			try
			{
				static_cast<void>(read_sprite_font_file(file.path()));
			}
			catch (const std::runtime_error& error)
			{
				rejected = true;
				const std::string message = error.what();
				CHECK(message.find(file.path()) != std::string::npos);
				CHECK(message.find(field.name) != std::string::npos);
				CHECK(message.find("must be finite") != std::string::npos);
				if (field.glyph_metric)
				{
					CHECK(message.find("glyph 0") != std::string::npos);
				}
			}
			CHECK(rejected);
		}
	}
}

TEST_CASE("spritefont preserves finite metric bit patterns without layout policy")
{
	const std::array<uint32_t, 12> finite_bits = {
		0x00000000u, 0x80000000u, 0x3fc00000u, 0xbfa00000u,
		0x7f7fffffu, 0xff7fffffu, 0x00800000u, 0x80800000u,
		0x00000001u, 0x80000001u, 0x3f800000u, 0xbf800000u
	};
	for (std::size_t row = 0; row < finite_bits.size(); ++row)
	{
		CAPTURE(row);
		std::vector<unsigned char> bytes =
			one_glyph_font(GlyphRectangleEdges{ 0, 0, 1, 1 });
		REQUIRE(bytes.size() == 76);
		std::array<uint32_t, 4> expected{};
		for (std::size_t field = 0; field < font_metric_fields.size(); ++field)
		{
			expected[field] = finite_bits[(row + field) % finite_bits.size()];
			overwrite_font_word(bytes, font_metric_fields[field].byte_offset,
				expected[field]);
		}
		const ScratchFile file("finite-font-metrics.spritefont", bytes);
		const SpriteFontFile font = read_sprite_font_file(file.path());
		REQUIRE(font.glyphs.size() == 1);
		const Glyph& glyph = font.glyphs[0];
		CHECK(glyph.character == U'A');
		CHECK(glyph.subrect == labrador::RectangleI(0, 0, 1, 1));
		CHECK(std::bit_cast<uint32_t>(glyph.x_offset) == expected[0]);
		CHECK(std::bit_cast<uint32_t>(glyph.y_offset) == expected[1]);
		CHECK(std::bit_cast<uint32_t>(glyph.x_advance) == expected[2]);
		CHECK(std::bit_cast<uint32_t>(font.line_spacing) == expected[3]);
		CHECK(font.atlas.width == 1);
		CHECK(font.atlas.height == 1);
		CHECK(font.atlas.pixels == std::vector<unsigned char>(4, 0xff));
	}
}

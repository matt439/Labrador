#include <doctest/doctest.h>

#include "engine/render/font.h"
#include "engine/render/text_encoding.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <stdexcept>
#include <string>
#include <vector>

// The pen, with no device and no file.
//
// THESE ARE THE ASSERTIONS RenderPixelTests CANNOT MAKE. That file draws a
// string and reads the pixels back, so it can only see the terms that reach
// the screen, only through one real font, and only on a machine that can build
// a Direct3D device. Every quirk in the walk is arithmetic; here it is
// arithmetic over a font invented for the question being asked, where a glyph
// can be exactly one texel wide and a line spacing can be a round number.
//
// A SECOND BACKEND INHERITS ALL OF THIS AND RE-RUNS NONE OF IT, which is the
// point of the walk having moved out of DirectXTK: font.cpp is compiled once
// per build, not once per backend, so these hold for every backend there will
// ever be. What a backend still owes is one Draw per glyph at the position
// this file pins.

namespace
{
	using labrador::Font;
	using labrador::Glyph;
	using labrador::TextureHandle;
	using mattmath::RectangleI;
	using mattmath::Vector2F;

	// A glyph whose cell is `width` by `height` with no bearing and no advance
	// adjustment, so its step is exactly its width until a test says otherwise.
	Glyph cell(char32_t character, int width, int height)
	{
		Glyph glyph;
		glyph.character = character;
		glyph.subrect = RectangleI(0, 0, width, height);
		return glyph;
	}

	// Ten wide, ten tall, stepping ten. Line spacing of twenty, so a line is
	// twice a glyph and the two are never confusable in a failure message.
	Font square_font(std::vector<Glyph> extra = {})
	{
		std::vector<Glyph> glyphs;
		glyphs.push_back(cell(U'A', 10, 10));
		glyphs.push_back(cell(U'B', 10, 10));
		for (const Glyph& glyph : extra)
		{
			glyphs.push_back(glyph);
		}
		return Font(TextureHandle(), std::move(glyphs), 20.0f);
	}

	// Every glyph the walk reports, in order, with where it put it.
	struct Placement
	{
		char32_t character = 0;
		float x = 0.0f;
		float y = 0.0f;
	};

	std::vector<Placement> walk(const Font& font, const std::wstring& text)
	{
		std::vector<Placement> placements;
		font.for_each_glyph(text,
			[&placements](const Glyph& glyph, const Vector2F& pen)
			{
				placements.push_back(Placement{ glyph.character, pen.x,
					pen.y });
			});
		return placements;
	}
}

TEST_CASE("a glyph table is looked up whatever order it was built in")
{
	std::vector<Glyph> glyphs;
	glyphs.push_back(cell(U'z', 1, 1));
	glyphs.push_back(cell(U'a', 2, 2));
	glyphs.push_back(cell(U'm', 3, 3));
	const Font font(TextureHandle(), std::move(glyphs), 10.0f);

	// The lookup is a binary search, so a file that listed its glyphs in any
	// other order would silently fail to find most of them if the constructor
	// trusted it. MakeSpriteFont happens to sort; nothing says it must.
	CHECK(font.contains(U'a'));
	CHECK(font.contains(U'm'));
	CHECK(font.contains(U'z'));
	CHECK_FALSE(font.contains(U'b'));
	CHECK(font.find(U'm')->subrect.width == 3);
}

TEST_CASE("the advance is the glyph's width plus its adjustment")
{
	Glyph tight = cell(U'C', 10, 10);
	tight.x_advance = -4.0f;

	Glyph loose = cell(U'D', 10, 10);
	loose.x_advance = 3.0f;

	std::vector<Glyph> extra;
	extra.push_back(tight);
	extra.push_back(loose);
	const Font font = square_font(std::move(extra));

	// THE TERM MOST LIKELY TO BE READ BACKWARDS. x_advance is an adjustment to
	// the advance, not the advance - most glyphs in a real atlas carry a
	// negative one, so a walk that used it as the step would draw every
	// character on top of the one before it and to the left.
	const std::vector<Placement> placements = walk(font, L"ACDA");
	REQUIRE(placements.size() == 4);
	CHECK(placements[0].x == doctest::Approx(0.0f));
	CHECK(placements[1].x == doctest::Approx(10.0f));
	CHECK(placements[2].x == doctest::Approx(16.0f));
	CHECK(placements[3].x == doctest::Approx(29.0f));
}

TEST_CASE("a bearing shifts the glyph and is not carried forward")
{
	Glyph indented = cell(U'C', 10, 10);
	indented.x_offset = 4.0f;
	indented.y_offset = 3.0f;

	std::vector<Glyph> extra;
	extra.push_back(indented);
	const Font font = square_font(std::move(extra));

	const std::vector<Placement> placements = walk(font, L"CC");
	REQUIRE(placements.size() == 2);

	// The bearing is in the reported x - it is where the glyph goes.
	CHECK(placements[0].x == doctest::Approx(4.0f));

	// AND IT ACCUMULATES INTO THE PEN, which is what makes it not a margin: the
	// second C is at 4 + 10 + 4, not at 10 + 4. A walk that applied the bearing
	// only for drawing would put it at 14 and drift a pixel per character
	// against what measure_text reported.
	CHECK(placements[1].x == doctest::Approx(18.0f));

	// The vertical bearing is NOT in the reported y. The walk reports the top
	// of the line and the caller adds y_offset, because measuring wants the
	// line and drawing wants the glyph - see font.h.
	CHECK(placements[0].y == doctest::Approx(0.0f));
}

TEST_CASE("a newline returns to the margin and drops one line spacing")
{
	const Font font = square_font();

	const std::vector<Placement> placements = walk(font, L"AB\nA\r\nB");
	REQUIRE(placements.size() == 4);

	CHECK(placements[1].x == doctest::Approx(10.0f));
	CHECK(placements[1].y == doctest::Approx(0.0f));

	CHECK(placements[2].x == doctest::Approx(0.0f));
	CHECK(placements[2].y == doctest::Approx(20.0f));

	// A carriage return before the newline is skipped rather than counted, so
	// a file with Windows line endings does not double-space.
	CHECK(placements[3].x == doctest::Approx(0.0f));
	CHECK(placements[3].y == doctest::Approx(40.0f));
}

TEST_CASE("whitespace steps the pen without being drawn, unless it is big")
{
	SUBCASE("a one-texel space draws nothing")
	{
		Glyph space = cell(U' ', 1, 1);
		space.x_offset = 5.0f;
		space.x_advance = 4.0f;

		std::vector<Glyph> extra;
		extra.push_back(space);
		const Font font = square_font(std::move(extra));

		// MakeSpriteFont writes exactly this glyph for U+0020, and it is not
		// obliged to be transparent - so a walk that reported it would put a
		// dot on the screen wherever a caller wrote a space.
		const std::vector<Placement> placements = walk(font, L"A A");
		REQUIRE(placements.size() == 2);
		CHECK(placements[0].character == U'A');
		CHECK(placements[1].character == U'A');

		// It still moved the pen: 10 for the A, then 5 of bearing and 5 of
		// advance for the space.
		CHECK(placements[1].x == doctest::Approx(20.0f));
	}

	SUBCASE("whitespace with a real cell is drawn")
	{
		std::vector<Glyph> extra;
		extra.push_back(cell(U' ', 6, 6));
		const Font font = square_font(std::move(extra));

		// The skip is a test on the glyph's size, not on the character alone.
		// An atlas that draws something for a space - an underline, a box in a
		// debug font - is asking for it to appear, and it does.
		const std::vector<Placement> placements = walk(font, L"A A");
		CHECK(placements.size() == 3);
	}
}

TEST_CASE("a character with no glyph draws the stand-in")
{
	std::vector<Glyph> extra;
	extra.push_back(cell(U'?', 8, 8));
	Font font = square_font(std::move(extra));

	// Before one is installed, drawing what the font has not got is fatal -
	// which is what the loader's stand-in exists to prevent, and which has to
	// stay reachable so that an atlas with no candidate says so rather than
	// silently drawing nothing.
	CHECK_THROWS_AS(walk(font, L"AZ"), std::runtime_error);

	font.set_stand_in(U'?');

	const std::vector<Placement> placements = walk(font, L"AZ");
	REQUIRE(placements.size() == 2);
	CHECK(placements[1].character == U'?');

	// The pen steps by the STAND-IN's width, not the missing glyph's, because
	// there is no missing glyph to have a width.
	CHECK(placements[1].x == doctest::Approx(10.0f));

	// And installing one the font has not got is itself the failure it was
	// meant to prevent, so it is reported at the moment of installing.
	CHECK_THROWS_AS(font.set_stand_in(U'\u00A3'), std::out_of_range);

	// first_unrenderable is asked of the atlas and not of what would happen -
	// substitution does not make a character renderable.
	CHECK(font.first_unrenderable(L"AZ") == 1);
	CHECK(font.first_unrenderable(L"AB") == std::wstring_view::npos);
}

TEST_CASE("a newline is not an unrenderable character")
{
	const Font font = square_font();

	// THE ATLAS HOLDS NO LINE FEED AND NEVER WILL. The walk answers U+000A
	// and U+000D itself, before it looks anything up, so a query that asked
	// the atlas about them reported every two-line string as unrenderable -
	// which is the opposite of the truth and is how a client discovered this,
	// by having its own content refused.
	CHECK(font.first_unrenderable(L"AB\nAB") == std::wstring_view::npos);
	CHECK(font.first_unrenderable(L"AB\r\nAB") == std::wstring_view::npos);
	CHECK(font.first_unrenderable(L"\n") == std::wstring_view::npos);

	// And the skip is a skip, not a stop: a character the font really has not
	// got is still found, and still reports its own index in the whole
	// string rather than an index into the run it sits in.
	CHECK(font.first_unrenderable(L"AB\nAZ") == 4);

	// The two the walk skips are exactly these two. Every other whitespace
	// character is a glyph lookup like any other - a tab draws the stand-in,
	// which is a thing to be seen and therefore a thing to report on.
	CHECK(font.first_unrenderable(L"A\tB") == 1);
}

TEST_CASE("a line is at least as tall as the font says a line is")
{
	std::vector<Glyph> extra;
	extra.push_back(cell(U'.', 4, 4));
	const Font font = square_font(std::move(extra));

	// A LINE OF FULL STOPS IS A LINE TALL. Measuring it as four would collapse
	// every layout that stacks measured text - a menu of short rows would
	// overlap itself - and the failure would look like a font problem.
	CHECK(font.measure(L"..").y == doctest::Approx(20.0f));
	CHECK(font.measure(L"..").x == doctest::Approx(8.0f));

	// The glyph wins when it is the taller of the two, which is what makes a
	// descender fit rather than being clipped by the line box.
	std::vector<Glyph> tall;
	tall.push_back(cell(U'g', 10, 26));
	const Font deep = square_font(std::move(tall));
	CHECK(deep.measure(L"g").y == doctest::Approx(26.0f));

	// Including its vertical bearing, because the bearing is how far down the
	// line the ink starts.
	Glyph dropped = cell(U'p', 10, 10);
	dropped.y_offset = 15.0f;
	std::vector<Glyph> below;
	below.push_back(dropped);
	const Font hanging = square_font(std::move(below));
	CHECK(hanging.measure(L"p").y == doctest::Approx(25.0f));
}

TEST_CASE("measuring is the width of the longest line and the bottom of the last")
{
	const Font font = square_font();

	CHECK(font.measure(L"").x == doctest::Approx(0.0f));
	CHECK(font.measure(L"").y == doctest::Approx(0.0f));

	CHECK(font.measure(L"AB").x == doctest::Approx(20.0f));
	CHECK(font.measure(L"AB").y == doctest::Approx(20.0f));

	// The longest line, not the last one - a two-line block is as wide as its
	// widest row, which is the number a centred label needs.
	CHECK(font.measure(L"ABA\nB").x == doctest::Approx(30.0f));
	CHECK(font.measure(L"ABA\nB").y == doctest::Approx(40.0f));
}

TEST_CASE("a surrogate pair is two code units, and the walk sees two of them")
{
	// THE ONE CROSSING BETWEEN text_encoding.h AND THIS FILE THAT NOTHING RAN.
	// widen() is the only producer of std::wstring in this engine, and on
	// Windows a wchar_t is sixteen bits - so a character outside the basic
	// plane arrives here as a surrogate PAIR, two code units neither of which
	// is a character. The walk casts each code unit straight to char32_t and
	// asks the atlas for it, which means a font that has neither half draws two
	// stand-ins where a reader would expect one glyph.
	//
	// That is a limitation rather than a defect, and it is here so that it is
	// a stated one: no font this engine loads has a glyph outside U+0020 to
	// U+007E (MakeSpriteFont's default region), so no atlas can contain either
	// half of a pair, and the alternative - decoding pairs in the walk - buys
	// nothing until an atlas does. docs/port/android.md prices the char16_t
	// question this belongs to.
	std::vector<Glyph> extra;
	extra.push_back(cell(U'?', 8, 8));
	Font font = square_font(std::move(extra));
	font.set_stand_in(U'?');

	// U+1F3AE VIDEO GAME, as UTF-8. widen() turns it into two code units.
	const std::wstring pair = labrador::widen("\xF0\x9F\x8E\xAE");
	REQUIRE(pair.size() == 2);

	// Neither half is renderable, and the FIRST one is where it says so - not
	// the pair, and not the second half.
	CHECK(font.first_unrenderable(pair) == 0);

	// And the pen steps twice, once per code unit, by the stand-in's width
	// each time.
	const std::vector<Placement> placements = walk(font, pair);
	REQUIRE(placements.size() == 2);
	CHECK(placements[0].character == U'?');
	CHECK(placements[1].character == U'?');
	CHECK(placements[0].x == doctest::Approx(0.0f));
	CHECK(placements[1].x == doctest::Approx(8.0f));

	// measure() agrees with the walk, because it is the walk - which is the
	// property that makes a caption box the right size for a string nobody
	// meant to write. Sixteen and not twenty: the step is the stand-in's own
	// eight-texel cell, not the ten of the glyphs that are there.
	CHECK(font.measure(pair).x == doctest::Approx(16.0f));
}

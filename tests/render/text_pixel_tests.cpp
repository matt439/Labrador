#include "tests/render/pixel_test_harness.h"

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace labrador;
using namespace render_tests::pixel;

// --- text ------------------------------------------------------------------

TEST_CASE("CONTRACT: the position is the top left of the text, and y runs down")
{
	Harness harness;

	// The string is two characters so that the box has a width the advance
	// contributes to, and both are capitals so nothing descends below the
	// baseline - a descender is the next case's business.
	const Vector2F size = harness.measure(L"AB");

	SUBCASE("every pixel of it is inside the box measure_text promised")
	{
		DrawList list = harness.begin();
		list.draw_text(harness.font, L"AB", Vector2F(12.0f, 8.0f),
			Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
		harness.end();

		const RectangleI ink = harness.ink_bounds();
		CHECK(ink.width > 0);

		// THIS IS THE TERM EVERY LAYOUT IN BOTH CLIENTS RESTS ON. A widget that
		// centres a label, a menu that sizes a row to its longest entry and the
		// drop shadow that offsets a copy of a string all ask measure_text for
		// a size and then trust draw_text to stay inside it. Nothing checked
		// that they agree, and they are computed by two different functions -
		// so a backend could measure with the line spacing and draw with the
		// glyph height and every label would be right until one of them
		// clipped.
		const RectangleI promised(12, 8,
			static_cast<int>(std::ceil(size.x)),
			static_cast<int>(std::ceil(size.y)));
		CHECK(promised.contains(ink));

		// And it is not merely inside it: the left edge of the first glyph is
		// the left edge of the box plus that glyph's own bearing, which is
		// small: its ink starts 4 pixels across and 5 down in this atlas,
		// so the second check passes by a single row, and a font with a taller
		// top bearing needs that bound looked at again. A backend that
		// centred the text in the box, or measured from the baseline, would
		// still pass the containment above.
		CHECK(ink.left() < 12 + 6);
		CHECK(ink.top() < 8 + 6);
	}

	SUBCASE("moving the position down moves the text down")
	{
		DrawList high = harness.begin();
		high.draw_text(harness.font, L"AB", Vector2F(12.0f, 8.0f),
			Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
		harness.end();
		const RectangleI first = harness.ink_bounds();

		DrawList low = harness.begin();
		low.draw_text(harness.font, L"AB", Vector2F(12.0f, 28.0f),
			Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
		harness.end();
		const RectangleI second = harness.ink_bounds();

		// Twenty pixels down, not twenty up. If y ran the other way for text -
		// which is the axis a second backend is most likely to get wrong,
		// because its own clip space runs up - this is negative.
		CHECK(second.top() - first.top() == 20);
		CHECK(second.left() == first.left());
	}
}

TEST_CASE("CONTRACT: the pen advances by what measure_text says it does")
{
	Harness harness;

	const Vector2F position(6.0f, 20.0f);

	DrawList single = harness.begin();
	single.draw_text(harness.font, L"A", position, Colour::white, 1.0f, 0.0f,
		Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI one = harness.ink_bounds();

	DrawList doubled = harness.begin();
	doubled.draw_text(harness.font, L"AA", position, Colour::white, 1.0f, 0.0f,
		Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI two = harness.ink_bounds();

	REQUIRE(one.width > 0);

	// A SECOND CHARACTER ADDS AN ADVANCE AND NOTHING ELSE. The second A sits on
	// the same rows as the first, so the box grows to the right and in no other
	// direction.
	CHECK(two.left() == one.left());
	CHECK(two.top() == one.top());
	CHECK(two.bottom() == one.bottom());

	// AND THE ADVANCE IS THE ONE MEASURE REPORTS, which is the only thing
	// making measure_text usable for anything but a single glyph. Exact rather
	// than within a pixel because this font is monospaced and its step is a
	// whole number; a proportional font would need a tolerance here and the
	// term would be no different.
	const int advance = static_cast<int>(
		harness.measure(L"AA").x - harness.measure(L"A").x);
	CHECK(advance > 0);
	CHECK(two.right() - one.right() == advance);
}

TEST_CASE("CONTRACT: a space advances the pen and draws nothing")
{
	Harness harness;

	const Vector2F position(4.0f, 20.0f);

	SUBCASE("a string of spaces marks the frame nowhere")
	{
		DrawList list = harness.begin();
		list.draw_text(harness.font, L"   ", position, Colour::white, 1.0f,
			0.0f, Vector2F::zero, 0.0f);
		harness.end();

		// THE ATLAS HAS A GLYPH FOR U+0020 AND IT IS NOT EMPTY. MakeSpriteFont
		// writes a one-texel subrect for it, and that texel is not guaranteed
		// to be transparent - so a backend that drew every glyph the pen walked
		// over would put a row of dots on the screen wherever a caller wrote a
		// space. Whitespace is skipped at draw time and only at draw time.
		CHECK(harness.ink_bounds() == RectangleI::zero);
	}

	SUBCASE("but the character after it has moved")
	{
		DrawList near_list = harness.begin();
		near_list.draw_text(harness.font, L"AA", position, Colour::white, 1.0f,
			0.0f, Vector2F::zero, 0.0f);
		harness.end();
		const RectangleI adjacent = harness.ink_bounds();

		DrawList far_list = harness.begin();
		far_list.draw_text(harness.font, L"A A", position, Colour::white, 1.0f,
			0.0f, Vector2F::zero, 0.0f);
		harness.end();
		const RectangleI spaced = harness.ink_bounds();

		// One more cell wide, and it starts in the same place.
		const int advance = static_cast<int>(
			harness.measure(L"AA").x - harness.measure(L"A").x);
		CHECK(spaced.left() == adjacent.left());
		CHECK(spaced.right() - adjacent.right() == advance);
	}
}

TEST_CASE("CONTRACT: a newline returns to the left margin and drops a line")
{
	Harness harness;

	const Vector2F position(4.0f, 4.0f);

	DrawList single = harness.begin();
	single.draw_text(harness.font, L"A", position, Colour::white, 1.0f, 0.0f,
		Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI one_line = harness.ink_bounds();

	DrawList wrapped = harness.begin();
	wrapped.draw_text(harness.font, L"A\nA", position, Colour::white, 1.0f,
		0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI two_lines = harness.ink_bounds();

	// Everything below compares two boxes, and two empty boxes agree about
	// everything. Say that both were drawn before believing they match.
	REQUIRE(one_line.width > 0);
	REQUIRE(two_lines.height > one_line.height);

	// Back to the margin, not to wherever the pen had reached.
	CHECK(two_lines.left() == one_line.left());
	CHECK(two_lines.right() == one_line.right());
	CHECK(two_lines.top() == one_line.top());

	// THE LINE SPACING IS THE FONT'S, NOT THE GLYPH'S. The font at this size
	// leaves eleven blank rows between two capitals, so a backend that stepped
	// by the glyph height would set the two lines touching and nothing would
	// throw - it would just look wrong in a way that reads as a font choice.
	// measure_text is asked what the step is, one pixel of tolerance because
	// the spacing is fractional and the second line therefore starts between
	// two rows.
	const float spacing = harness.measure(L"A\nA").y - harness.measure(L"A").y;
	CHECK(spacing > 0.0f);
	CHECK(std::abs(static_cast<float>(two_lines.bottom() - one_line.bottom())
		- spacing) <= 1.0f);

	// And there is a gap, so the two lines are two lines.
	CHECK_FALSE(harness.row_has_ink(one_line.bottom() + 1));
}

TEST_CASE("CONTRACT: the tint multiplies the glyph, as it does a sprite")
{
	Harness harness;

	const Vector2F position(8.0f, 20.0f);

	DrawList plain = harness.begin();
	plain.draw_text(harness.font, L"Ag", position, Colour::white, 1.0f, 0.0f,
		Vector2F::zero, 0.0f);
	harness.end();
	const std::vector<Pixel> white_text = harness.snapshot();

	DrawList tinted = harness.begin();
	tinted.draw_text(harness.font, L"Ag", position,
		Colour(1.0f, 0.0f, 0.0f, 1.0f), 1.0f, 0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const std::vector<Pixel> red_text = harness.snapshot();

	// EVERY PIXEL AT ONCE, AND AGGREGATED INTO THREE ASSERTIONS. A glyph's
	// coverage differs pixel by pixel and this file has no business knowing
	// what any one of those values is; what it can say is that multiplying by
	// pure red leaves the red channel exactly alone and empties the other two.
	// A tint that replaced rather than multiplied would flood the whole glyph
	// box with solid red and fail the first of these.
	int ink_pixels = 0;
	bool red_preserved = true;
	bool other_channels_cleared = true;

	for (size_t i = 0; i < white_text.size(); i++)
	{
		if (white_text[i] == black_pixel)
		{
			// Nothing was drawn here, so nothing may appear here either.
			other_channels_cleared = other_channels_cleared &&
				red_text[i] == black_pixel;
			continue;
		}
		ink_pixels++;
		red_preserved = red_preserved && red_text[i].r == white_text[i].r;
		other_channels_cleared = other_channels_cleared &&
			red_text[i].g == 0 && red_text[i].b == 0;
	}

	CHECK(ink_pixels > 0);
	CHECK(red_preserved);
	CHECK(other_channels_cleared);
}

TEST_CASE("CONTRACT: text scales about the origin, in unscaled text pixels")
{
	Harness harness;

	// High enough that the doubled glyph ends clear of the bottom of the
	// frame. A box that reached the last row could not tell a glyph that is
	// twice the height from one that is taller still and clipped to it.
	const Vector2F position(24.0f, 12.0f);

	DrawList plain_list = harness.begin();
	plain_list.draw_text(harness.font, L"A", position, Colour::white, 1.0f,
		0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI plain = harness.ink_bounds();

	DrawList doubled_list = harness.begin();
	doubled_list.draw_text(harness.font, L"A", position, Colour::white, 2.0f,
		0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI doubled = harness.ink_bounds();

	REQUIRE(plain.width > 0);

	// Twice the size, and pinned at the position rather than about its own
	// centre. Exact: point sampling at a whole-number scale gives every texel
	// exactly two pixels, so the anti-aliased edge doubles along with the rest.
	CHECK(doubled.width == plain.width * 2);
	CHECK(doubled.height == plain.height * 2);
	CHECK(doubled.left() - static_cast<int>(position.x) ==
		(plain.left() - static_cast<int>(position.x)) * 2);
	CHECK(doubled.top() - static_cast<int>(position.y) ==
		(plain.top() - static_cast<int>(position.y)) * 2);

	DrawList shifted = harness.begin();
	shifted.draw_text(harness.font, L"A", position, Colour::white, 2.0f, 0.0f,
		Vector2F(8.0f, 4.0f), 0.0f);
	harness.end();
	const RectangleI moved = harness.ink_bounds();

	// THE SAME TERM THE SPRITE CASE FIXES, and it is worth fixing twice because
	// the two travel through different arithmetic: a sprite's origin is in
	// source texels and is scaled by the ratio of destination to source, while
	// text has no destination rectangle at all and its origin is in the
	// unscaled pen space measure_text reports in. An origin of eight text
	// pixels at a scale of two moves the text sixteen screen pixels, not eight.
	CHECK(moved.left() == doubled.left() - 16);
	CHECK(moved.top() == doubled.top() - 8);
	CHECK(moved.width == doubled.width);
}

TEST_CASE("CONTRACT: point-sampled text off a whole pixel lands on the nearest one")
{
	Harness harness;

	// A quarter of a pixel each side of a whole one, in both axes and in both
	// directions. Under the point filter at a scale of one, a glyph at x + 0.25
	// covers the same pixel centres as one at x and samples the same texels at
	// them, a quarter of a texel in from the edge; at x + 0.75 it is the same
	// statement about x + 1. So text centred by halving a measured width draws
	// the frame it would draw on a whole pixel, and nothing about where it went
	// is left to the backend - the y term included, which on gl/ is measured
	// from the other end of the buffer.
	//
	// HALF A PIXEL IS NOT HERE, AND CANNOT BE. At x + 0.5 every pixel centre is
	// exactly a texel edge, and which of the two texels the rasteriser's
	// interpolated coordinate rounds into is no API's term. It differs between
	// backends on one GPU, and between the two triangles of one quad, so a
	// glyph there can come out torn along its diagonal. renderer.h tells a
	// caller to put point-sampled text on a whole pixel instead.
	const Vector2F quarter(4.25f, 6.75f);
	const Vector2F three_quarters(30.75f, 34.25f);

	DrawList fractional = harness.begin();
	fractional.set_filter(TextureFilter::point);
	fractional.draw_text(harness.font, L"Ag", quarter, Colour::white, 1.0f,
		0.0f, Vector2F::zero, 0.0f);
	fractional.draw_text(harness.font, L"Ag", three_quarters, Colour::white,
		1.0f, 0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const std::vector<Pixel> off_whole = harness.snapshot();
	const RectangleI off_whole_ink = harness.ink_bounds();

	DrawList whole = harness.begin();
	whole.set_filter(TextureFilter::point);
	whole.draw_text(harness.font, L"Ag", Vector2F(4.0f, 7.0f), Colour::white,
		1.0f, 0.0f, Vector2F::zero, 0.0f);
	whole.draw_text(harness.font, L"Ag", Vector2F(31.0f, 34.0f),
		Colour::white, 1.0f, 0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const std::vector<Pixel> on_whole = harness.snapshot();

	// Two drawn frames, so that the comparison below is not two empty ones.
	REQUIRE(off_whole_ink.width > 0);

	// EVERY PIXEL, NOT A BOX. The failure this pins is a texel picked one
	// column or row over inside a glyph, which leaves the ink's bounds where
	// they were.
	int differing = 0;
	for (size_t i = 0; i < on_whole.size(); i++)
	{
		if (!(off_whole[i] == on_whole[i]))
		{
			differing++;
		}
	}
	CHECK(differing == 0);
}

TEST_CASE("CONTRACT: the camera maps text position and text scale together")
{
	Harness harness;

	DrawList screen = harness.begin();
	screen.draw_text(harness.font, L"A", Vector2F(6.0f, 10.0f), Colour::white,
		2.0f, 0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI direct = harness.ink_bounds();

	DrawList world = harness.begin();

	// view = (world - translation) * scale, so world (19, 13) under this camera
	// is screen (6, 10) and a text scale of one is a screen scale of two.
	world.set_camera(Camera(16.0f, 8.0f, 2.0f));
	world.draw_text(harness.font, L"A", Vector2F(19.0f, 13.0f), Colour::white,
		1.0f, 0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const RectangleI through_camera = harness.ink_bounds();

	// As in the newline case: two empty boxes are equal, so check there is
	// something to compare before comparing it.
	REQUIRE(direct.width > 0);
	REQUIRE(through_camera.width > 0);

	// BOTH HALVES, WHICH IS WHY THE CAMERA HAS A ZOOM AT ALL. A backend that
	// applied the camera to the position and forgot the scale would put the
	// text in exactly the right place at exactly the wrong size, which looks
	// like a font problem and is not one.
	CHECK(through_camera.left() == direct.left());
	CHECK(through_camera.top() == direct.top());
	CHECK(through_camera.width == direct.width);
	CHECK(through_camera.height == direct.height);
}

TEST_CASE("CONTRACT: a character the font lacks draws the stand-in glyph")
{
	Harness harness;

	const Vector2F position(20.0f, 20.0f);

	DrawList question = harness.begin();
	question.draw_text(harness.font, L"?", position, Colour::white, 1.0f, 0.0f,
		Vector2F::zero, 0.0f);
	harness.end();
	const std::vector<Pixel> expected = harness.snapshot();

	DrawList curly = harness.begin();
	// U+2019, the character a word processor inserts for an apostrophe. It is
	// outside the 32..126 region MakeSpriteFont writes when nobody chooses one,
	// which is every .spritefont in this tree. Spelt as an escape so that this
	// file stays ASCII and no encoding decision can change what is tested.
	curly.draw_text(harness.font, L"\u2019", position, Colour::white, 1.0f,
		0.0f, Vector2F::zero, 0.0f);
	harness.end();
	const std::vector<Pixel> drawn = harness.snapshot();

	// NOT AN EXCEPTION AND NOT A HOLE. Before the loader installed a default
	// character this threw on the frame that first drew the string, several
	// screens from wherever the text was read. The seam's own promise, that
	// text about to be drawn shows mojibake rather than vanishing, is this
	// assertion: the frame is pixel for pixel the frame a question mark draws.
	CHECK(harness.ink_bounds().width > 0);
	CHECK((drawn == expected));
}

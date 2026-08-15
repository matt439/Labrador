#include <doctest/doctest.h>

#include "engine/render/sprite_geometry.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_vertex.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <cmath>

// Where a sprite's corners go, with no device.
//
// THE SAME TERMS RenderPixelTests PINS, ASKED A DIFFERENT WAY. That file draws
// and reads the pixels back, which is the only proof that the whole path agrees
// - but it can only see a term through a rasteriser, at whole-pixel resolution,
// on a machine that can build a Direct3D device. This file reads the four
// corners directly, so it can assert a fractional position, a rotation, and a
// texture coordinate, none of which a 64x64 read-back can separate from its
// neighbours.
//
// NEITHER REPLACES THE OTHER. If these pass and the pixel tests fail, the
// backend is wrong; if these fail, the arithmetic is, and every backend is
// wrong at once. That is the whole reason the arithmetic moved out of the
// backend.

namespace
{
	using namespace labrador;
	using namespace mattmath;

	// A texture big enough that a source rectangle inside it gives texture
	// coordinates worth reading: 16 texels means a quarter is 0.25 exactly, so
	// nothing below is comparing against a repeating fraction.
	const Vector2F TEXTURE(16.0f, 16.0f);

	// The whole of a 16x16 texture, for the cases that are not about the
	// source rectangle.
	RectangleI whole() { return RectangleI(0, 0, 16, 16); }

	// A glyph four texels by eight with the bearing the case is about. Four by
	// eight rather than square so a term applied to the wrong axis shows as a
	// wrong number and not as a coincidence.
	Glyph bearing(float y_offset)
	{
		Glyph glyph;
		glyph.character = U'A';
		glyph.subrect = RectangleI(0, 0, 4, 8);
		glyph.y_offset = y_offset;
		return glyph;
	}
}

TEST_CASE("CONTRACT: the corners are the destination's, in the fixed order")
{
	SpriteVertex corners[4];
	build_sprite_quad(RectangleF(5.0f, 7.0f, 10.0f, 20.0f), whole(), TEXTURE,
		Colour::white, 0.0f, Vector2F::ZERO, SpriteFlip::none, corners);

	// Top left, top right, bottom left, bottom right - which is what the
	// backend's index buffer is built from, and the one thing about this
	// function a backend cannot discover for itself.
	CHECK(corners[0].position.x == doctest::Approx(5.0f));
	CHECK(corners[0].position.y == doctest::Approx(7.0f));
	CHECK(corners[1].position.x == doctest::Approx(15.0f));
	CHECK(corners[1].position.y == doctest::Approx(7.0f));
	CHECK(corners[2].position.x == doctest::Approx(5.0f));
	CHECK(corners[2].position.y == doctest::Approx(27.0f));
	CHECK(corners[3].position.x == doctest::Approx(15.0f));
	CHECK(corners[3].position.y == doctest::Approx(27.0f));
}

TEST_CASE("CONTRACT: the source rectangle is in texels and texel (0,0) is top left")
{
	SpriteVertex corners[4];
	build_sprite_quad(RectangleF(0.0f, 0.0f, 8.0f, 8.0f),
		RectangleI(4, 8, 4, 4), TEXTURE, Colour::white, 0.0f, Vector2F::ZERO,
		SpriteFlip::none, corners);

	// A source of (4,8) size 4x4 out of a 16x16 texture is the quarter starting
	// at a quarter across and half way down. The top-left corner of the
	// destination samples the top-left corner of the source, which is the
	// assertion a y-inverted texture coordinate fails.
	CHECK(corners[0].texcoord.x == doctest::Approx(0.25f));
	CHECK(corners[0].texcoord.y == doctest::Approx(0.5f));
	CHECK(corners[3].texcoord.x == doctest::Approx(0.5f));
	CHECK(corners[3].texcoord.y == doctest::Approx(0.75f));
}

TEST_CASE("CONTRACT: a flip mirrors the texture and leaves the rectangle alone")
{
	const RectangleF destination(0.0f, 0.0f, 8.0f, 8.0f);
	const RectangleI source(0, 0, 8, 8);

	SpriteVertex plain[4];
	build_sprite_quad(destination, source, TEXTURE, Colour::white, 0.0f,
		Vector2F::ZERO, SpriteFlip::none, plain);

	SpriteVertex flipped[4];

	SUBCASE("horizontal swaps the left and right coordinates")
	{
		build_sprite_quad(destination, source, TEXTURE, Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::horizontal, flipped);

		// THE POSITIONS DO NOT MOVE. Mirroring by negating the destination size
		// would put the sprite somewhere else entirely and look identical for a
		// symmetrical texture, which is most of them.
		for (int i = 0; i < 4; i++)
		{
			CHECK(flipped[i].position.x ==
				doctest::Approx(plain[i].position.x));
			CHECK(flipped[i].position.y ==
				doctest::Approx(plain[i].position.y));
		}

		CHECK(flipped[0].texcoord.x == doctest::Approx(plain[1].texcoord.x));
		CHECK(flipped[0].texcoord.y == doctest::Approx(plain[0].texcoord.y));
	}

	SUBCASE("vertical swaps the top and bottom coordinates")
	{
		build_sprite_quad(destination, source, TEXTURE, Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::vertical, flipped);

		CHECK(flipped[0].texcoord.x == doctest::Approx(plain[0].texcoord.x));
		CHECK(flipped[0].texcoord.y == doctest::Approx(plain[2].texcoord.y));
	}

	SUBCASE("both is the diagonally opposite corner")
	{
		build_sprite_quad(destination, source, TEXTURE, Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::both, flipped);

		CHECK(flipped[0].texcoord.x == doctest::Approx(plain[3].texcoord.x));
		CHECK(flipped[0].texcoord.y == doctest::Approx(plain[3].texcoord.y));
	}
}

TEST_CASE("CONTRACT: each destination edge truncates, and the size follows")
{
	SpriteVertex corners[4];
	build_sprite_quad(RectangleF(10.9f, 0.0f, 8.5f, 4.0f), whole(), TEXTURE,
		Colour::white, 0.0f, Vector2F::ZERO, SpriteFlip::none, corners);

	// THE EDGES TRUNCATE, NOT THE POSITION AND THE SIZE, and this rectangle is
	// chosen so the two answers differ. Its right edge is 19.4, which truncates
	// to 19, so the width is 9 - where truncating the size would give 8 and be
	// a pixel narrow for every sprite whose position has a fraction. The pixel
	// tests cannot tell those apart without a case built for it; this can.
	CHECK(corners[0].position.x == doctest::Approx(10.0f));
	CHECK(corners[1].position.x == doctest::Approx(19.0f));

	// And it rounds towards zero rather than to nearest: 10.9 is 10, not 11.
	CHECK(corners[0].position.x < 10.5f);
}

TEST_CASE("CONTRACT: the origin is in unscaled source texels")
{
	SpriteVertex corners[4];
	build_sprite_quad(RectangleF(8.0f, 8.0f, 8.0f, 8.0f),
		RectangleI(0, 0, 2, 2), Vector2F(2.0f, 2.0f), Colour::white, 0.0f,
		Vector2F(2.0f, 2.0f), SpriteFlip::none, corners);

	// An 8x8 destination over a 2x2 source is a scale of four, so an origin of
	// two source texels shifts the sprite eight destination pixels up and left -
	// putting its top-left corner exactly at the origin of the view. If origin
	// were measured in destination pixels the shift would be two.
	CHECK(corners[0].position.x == doctest::Approx(0.0f));
	CHECK(corners[0].position.y == doctest::Approx(0.0f));
	CHECK(corners[3].position.x == doctest::Approx(8.0f));
}

TEST_CASE("CONTRACT: rotation turns about the position, clockwise on screen")
{
	SpriteVertex corners[4];
	const float QUARTER_TURN = 1.57079633f;

	build_sprite_quad(RectangleF(0.0f, 0.0f, 10.0f, 4.0f), whole(), TEXTURE,
		Colour::white, QUARTER_TURN, Vector2F::ZERO, SpriteFlip::none,
		corners);

	// NOTHING PINS THIS ANYWHERE ELSE. RenderPixelTests says in its own header
	// that rotation is one of the terms it does not cover, because a rotated
	// glyph or sprite read back at 64x64 is a smear rather than an assertion.
	// The corner positions are exact, so the direction can be fixed here: a
	// quarter turn takes the top-right corner from ten pixels right of the
	// position to ten pixels below it, which is clockwise when y runs down.
	CHECK(corners[1].position.x == doctest::Approx(0.0f).epsilon(0.001));
	CHECK(corners[1].position.y == doctest::Approx(10.0f).epsilon(0.001));

	// And the corner opposite the pivot ends up where both offsets take it.
	CHECK(corners[2].position.x == doctest::Approx(-4.0f).epsilon(0.001));
	CHECK(corners[2].position.y == doctest::Approx(0.0f).epsilon(0.001));

	// The pivot itself does not move, which is what "about the position" means.
	CHECK(corners[0].position.x == doctest::Approx(0.0f));
	CHECK(corners[0].position.y == doctest::Approx(0.0f));
}

TEST_CASE("CONTRACT: the tint is on every corner, unchanged")
{
	SpriteVertex corners[4];
	const Colour tint(0.25f, 0.5f, 0.75f, 0.5f);

	build_sprite_quad(RectangleF(0.0f, 0.0f, 4.0f, 4.0f), whole(), TEXTURE,
		tint, 0.0f, Vector2F::ZERO, SpriteFlip::none, corners);

	// Flat, not interpolated. A tint that varied across the quad would be a
	// gradient, and the seam offers one colour per draw.
	for (int i = 0; i < 4; i++)
	{
		CHECK(corners[i].colour.r == doctest::Approx(0.25f));
		CHECK(corners[i].colour.g == doctest::Approx(0.5f));
		CHECK(corners[i].colour.b == doctest::Approx(0.75f));
		CHECK(corners[i].colour.a == doctest::Approx(0.5f));
	}
}

TEST_CASE("CONTRACT: the scaled form keeps its fraction, and scales the source")
{
	SpriteVertex corners[4];
	build_scaled_quad(Vector2F(1.5f, 2.5f), 2.0f, RectangleI(0, 0, 3, 4),
		TEXTURE, Colour::white, 0.0f, Vector2F::ZERO, corners);

	// NO TRUNCATION, WHICH IS THE WHOLE DIFFERENCE FROM THE FORM ABOVE. Text is
	// laid out through this: a pen advance is fractional in most fonts, and
	// rounding each glyph to a whole pixel would make a line drift against its
	// own measurement.
	CHECK(corners[0].position.x == doctest::Approx(1.5f));
	CHECK(corners[0].position.y == doctest::Approx(2.5f));

	// The size is the source's, scaled - not the texture's.
	CHECK(corners[3].position.x == doctest::Approx(7.5f));
	CHECK(corners[3].position.y == doctest::Approx(10.5f));
}

TEST_CASE("a source rectangle with no area does not divide by nothing")
{
	SpriteVertex corners[4];
	build_scaled_quad(Vector2F(4.0f, 4.0f), 1.0f, RectangleI(0, 0, 0, 0),
		TEXTURE, Colour::white, 0.0f, Vector2F(1.0f, 1.0f), corners);

	// The origin is divided by the source size to become a fraction of the
	// destination, so a zero-area source is a division by nothing. It draws
	// nothing whatever the answer is - the destination is zero-sized too - but
	// "nothing" and "a NaN in a vertex buffer" are different things to hand a
	// driver.
	for (int i = 0; i < 4; i++)
	{
		CHECK(std::isfinite(corners[i].position.x));
		CHECK(std::isfinite(corners[i].position.y));
	}
}

// --- the glyph form ---------------------------------------------------------
//
// THE TERM BELOW WAS PINNED BY NOTHING UNTIL THIS BLOCK EXISTED, and it was
// pinned by nothing in a way worth writing down, because the obvious reading of
// the test suite says otherwise. RenderPixelTests draws nine strings; every one
// of them compares two ink boxes drawn by the same code, so a term added
// equally to every glyph in both cancels out of the comparison. The one case
// that measures against an absolute rectangle cannot fail either, because a
// line is at least line_spacing tall and RectangleI::contains is inclusive, so
// the ink has room to move inside its own promised box. The null backend's
// record test compares 'A' against 'B', which in a real font carry the SAME
// bearing. Deleting glyph.y_offset from any one backend therefore left every
// configuration green - which is what put the arithmetic here.

TEST_CASE("CONTRACT: a glyph's bearing lowers it, and scales with the text")
{
	SpriteVertex flush[4];
	SpriteVertex dropped[4];

	// Same pen, same everything, one bearing apart. So the difference between
	// the two is the bearing and can be nothing else.
	build_glyph_quad(Vector2F(100.0f, 200.0f), 3.0f, bearing(0.0f),
		Vector2F(10.0f, 20.0f), TEXTURE, Colour::white, 0.0f, Vector2F::ZERO,
		flush);
	build_glyph_quad(Vector2F(100.0f, 200.0f), 3.0f, bearing(5.0f),
		Vector2F(10.0f, 20.0f), TEXTURE, Colour::white, 0.0f, Vector2F::ZERO,
		dropped);

	// A BEARING IS IN UNSCALED SOURCE TEXELS LIKE EVERY OTHER ORIGIN TERM, so
	// five texels at a scale of three is fifteen pixels and not five. This is
	// the assertion that fails if the term is dropped from the quad, and the
	// reason it is stated as a difference rather than as a position.
	CHECK(dropped[0].position.y - flush[0].position.y ==
		doctest::Approx(15.0f));

	// Down, not up. The walk reports pen.y as the top of the LINE, and the
	// bearing is how far below that top this particular glyph's cell begins -
	// so a capital with a large bearing sits lower, not higher.
	CHECK(dropped[0].position.y > flush[0].position.y);

	// It is a y term alone. Nothing about a bearing moves a glyph sideways.
	CHECK(dropped[0].position.x == doctest::Approx(flush[0].position.x));
}

TEST_CASE("CONTRACT: the pen places the glyph and the cell rides with it")
{
	SpriteVertex corners[4];
	build_glyph_quad(Vector2F(100.0f, 200.0f), 3.0f, bearing(5.0f),
		Vector2F(10.0f, 20.0f), TEXTURE, Colour::white, 0.0f, Vector2F::ZERO,
		corners);

	// The pen and the bearing are both scaled: x is 100 + 10*3, y is
	// 200 + (20 + 5)*3. An implementation that scaled the cell but not the pen
	// would pass every relative assertion in RenderPixelTests and fail here.
	CHECK(corners[0].position.x == doctest::Approx(130.0f));
	CHECK(corners[0].position.y == doctest::Approx(275.0f));

	// And the cell is still the cell: four by eight source texels at three.
	CHECK(corners[3].position.x - corners[0].position.x ==
		doctest::Approx(12.0f));
	CHECK(corners[3].position.y - corners[0].position.y ==
		doctest::Approx(24.0f));
}

TEST_CASE("CONTRACT: the caller's origin is the string's, in unscaled texels")
{
	SpriteVertex at_zero[4];
	SpriteVertex shifted[4];

	build_glyph_quad(Vector2F(100.0f, 200.0f), 3.0f, bearing(5.0f),
		Vector2F(10.0f, 20.0f), TEXTURE, Colour::white, 0.0f, Vector2F::ZERO,
		at_zero);
	build_glyph_quad(Vector2F(100.0f, 200.0f), 3.0f, bearing(5.0f),
		Vector2F(10.0f, 20.0f), TEXTURE, Colour::white, 0.0f,
		Vector2F(2.0f, 3.0f), shifted);

	// An origin moves the string against its position, so it subtracts - and
	// it is in the same unscaled source texels the pen and the bearing are, so
	// two texels at a scale of three is six pixels. The whole reason the pen
	// rides in the origin is that the two compose without a conversion; this
	// is the case that says so.
	CHECK(at_zero[0].position.x - shifted[0].position.x ==
		doctest::Approx(6.0f));
	CHECK(at_zero[0].position.y - shifted[0].position.y ==
		doctest::Approx(9.0f));
}

TEST_CASE("CONTRACT: a rotated line turns about the string, not each glyph")
{
	SpriteVertex corners[4];
	const float QUARTER_TURN = 1.57079633f;

	build_glyph_quad(Vector2F(100.0f, 200.0f), 2.0f, bearing(0.0f),
		Vector2F(10.0f, 0.0f), TEXTURE, Colour::white, QUARTER_TURN,
		Vector2F::ZERO, corners);

	// Ten texels along the pen at a scale of two is twenty pixels, and a
	// quarter turn about the STRING'S position puts them below it rather than
	// right of it. Baking the pen into the position instead of the origin -
	// which is the obvious simplification, and reads identically at a rotation
	// of zero - turns each glyph about its own cell and leaves this at
	// (120, 200).
	CHECK(corners[0].position.x == doctest::Approx(100.0f).epsilon(0.001));
	CHECK(corners[0].position.y == doctest::Approx(220.0f).epsilon(0.001));
}

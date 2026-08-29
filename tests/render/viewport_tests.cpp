#include <doctest/doctest.h>

#include "engine/render/viewport.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"

// Which whole pixels a viewport covers, with no device.
//
// ONE CONVERSION, ON THE SEAM, BECAUSE A BACKEND ANSWERING IT SEPARATELY CAN
// DISAGREE WITH ITSELF. GL 3.3 core has no float glViewport, so the GL backend
// has to reach whole pixels somehow; truncating the position and the size
// independently for the rasteriser and then dividing by the UN-truncated float
// to build the pixels-to-clip transform hands the rasteriser and the
// projection two different viewport sizes, and scales every sprite in a
// fractional pane by the ratio between them. D3D11 cannot have that bug - it
// passes one D3D11_VIEWPORT to both consumers - which is precisely why it is
// hard to catch: RenderPixelTests creates one integral 64x64 view, both samples
// take their resolution from a Vector2I, and set_viewport has exactly one
// caller in the whole tree. The trigger is split-screen on an odd client
// extent, which lives in a client repository and not in this one.
//
// SO THE CONVERSION IS THE ENGINE'S NOW, and this file is what says what it
// answers. It needs no device and no driver, so it runs in every configuration
// including the null one - which is the only configuration CI can run
// completely, and the one where nothing downstream of glViewport exists to be
// tested at all.

namespace
{
	using labrador::Viewport;
	using mattmath::RectangleF;
	using mattmath::RectangleI;
}

TEST_CASE("CONTRACT: a whole-pixel viewport is already its own pixel rect")
{
	const RectangleI pixels = Viewport(10.0f, 20.0f, 640.0f, 480.0f).pixel_rect();

	CHECK(pixels.x == 10);
	CHECK(pixels.y == 20);
	CHECK(pixels.width == 640);
	CHECK(pixels.height == 480);
}

TEST_CASE("CONTRACT: each edge truncates, and the size is the difference")
{
	// A pane at 10.9 that is 8 wide has a right edge of 18.9. Truncating the
	// two edges gives 10 and 18, so a width of 8; truncating the position and
	// the size separately gives 10 and 8, so a right edge of 18 - the same
	// answer here by luck. The case below is the one where they differ.
	const RectangleI pixels = Viewport(10.9f, 0.0f, 8.0f, 4.0f).pixel_rect();

	CHECK(pixels.x == 10);
	CHECK(pixels.width == 8);

	// The rule is sprite_geometry.h's, said about a bigger rectangle: a
	// destination's edges truncate and the size is their difference. One rule,
	// stated once, and the reason a viewport and a sprite cannot drift apart.
	const RectangleI narrower = Viewport(10.9f, 0.0f, 7.5f, 4.0f).pixel_rect();

	CHECK(narrower.x == 10);
	CHECK(narrower.width == 8);
}

TEST_CASE("CONTRACT: panes that split an odd extent still cover every row")
{
	// THE CASE THE OLD ARITHMETIC LOST A ROW ON, and the reason this file
	// exists. A client height of 721 split two ways gives two panes 360.5 tall.
	const Viewport top(0.0f, 0.0f, 1280.0f, 360.5f);
	const Viewport bottom(0.0f, 360.5f, 1280.0f, 360.5f);

	const RectangleI top_pixels = top.pixel_rect();
	const RectangleI bottom_pixels = bottom.pixel_rect();

	// 360 and 361, not 360 and 360. Truncating each size independently gives
	// the second pair, which leaves the back buffer's last row covered by
	// neither pane - permanently the clear colour, on one backend only.
	CHECK(top_pixels.height == 360);
	CHECK(bottom_pixels.height == 361);
	CHECK(top_pixels.height + bottom_pixels.height == 721);

	// And they meet exactly: no row is covered twice either.
	CHECK(bottom_pixels.y == top_pixels.y + top_pixels.height);
}

TEST_CASE("CONTRACT: three panes over an extent that divides into none of them")
{
	// Two panes can be made to work by rounding one of them; three is where an
	// independent per-pane rule stops being rescuable. 1000 into thirds.
	const float third = 1000.0f / 3.0f;

	const RectangleI first = Viewport(0.0f, 0.0f, third, 100.0f).pixel_rect();
	const RectangleI second =
		Viewport(third, 0.0f, third, 100.0f).pixel_rect();
	const RectangleI last =
		Viewport(third * 2.0f, 0.0f, third, 100.0f).pixel_rect();

	CHECK(first.width + second.width + last.width == 1000);
	CHECK(second.x == first.x + first.width);
	CHECK(last.x == second.x + second.width);
}

TEST_CASE("a viewport with no area has no pixels, and does not go negative")
{
	const RectangleI empty = Viewport(5.0f, 5.0f, 0.0f, 0.0f).pixel_rect();

	// A backend divides by these to build its transform and guards against
	// zero; a negative would clear that guard and put the projection through
	// a sign flip nobody asked for.
	CHECK(empty.width == 0);
	CHECK(empty.height == 0);

	// Narrower than one pixel and lying entirely inside it is still no pixels,
	// which is the honest answer: the rasteriser cannot fill part of one.
	const RectangleI sliver = Viewport(5.2f, 5.2f, 0.5f, 0.5f).pixel_rect();

	CHECK(sliver.width == 0);
	CHECK(sliver.height == 0);
}

TEST_CASE("the depth range is not a pixel and does not come along")
{
	// pixel_rect answers one question. minDepth and maxDepth are the other
	// half of a Viewport and every backend still reads them straight off,
	// so a conversion that quietly dropped or clamped them would be a
	// different bug in the same call.
	const Viewport viewport(0.0f, 0.0f, 32.0f, 32.0f, 0.25f, 0.75f);

	CHECK(viewport.minDepth == doctest::Approx(0.25f));
	CHECK(viewport.maxDepth == doctest::Approx(0.75f));
	CHECK(viewport.pixel_rect().width == 32);
}

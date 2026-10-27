#include <doctest/doctest.h>

#include "samples/components/nine_slice.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"

#include <array>
#include <stdexcept>

using components::NineSlice;
using components::SliceBorder;
using components::SlicePiece;
using components::nine_slice;
using labrador::Colour;
using labrador::RectangleF;
using labrador::RectangleI;
using labrador::TextureHandle;

namespace
{
	// A 16-texel square at (32, 0) in its atlas, with a 6-texel corner on the
	// left and top and a 4-texel one on the right and bottom, so a piece read
	// from the wrong side has the wrong size.
	const RectangleI frame(32, 0, 16, 16);
	const SliceBorder border{ 6, 6, 4, 4 };

	void check_rectangle(const RectangleF& actual, float x, float y,
		float width, float height)
	{
		CHECK(actual.x == doctest::Approx(x));
		CHECK(actual.y == doctest::Approx(y));
		CHECK(actual.width == doctest::Approx(width));
		CHECK(actual.height == doctest::Approx(height));
	}
}

TEST_CASE("the corners keep their size and the rest stretches")
{
	const std::array<SlicePiece, 9> pieces =
		nine_slice(frame, border, RectangleF(10.0f, 20.0f, 100.0f, 40.0f));

	// Sources: the corners, edges and centre of the frame, in reading order.
	CHECK(pieces[0].source == RectangleI(32, 0, 6, 6));
	CHECK(pieces[1].source == RectangleI(38, 0, 6, 6));
	CHECK(pieces[2].source == RectangleI(44, 0, 4, 6));
	CHECK(pieces[3].source == RectangleI(32, 6, 6, 6));
	CHECK(pieces[4].source == RectangleI(38, 6, 6, 6));
	CHECK(pieces[5].source == RectangleI(44, 6, 4, 6));
	CHECK(pieces[6].source == RectangleI(32, 12, 6, 4));
	CHECK(pieces[7].source == RectangleI(38, 12, 6, 4));
	CHECK(pieces[8].source == RectangleI(44, 12, 4, 4));

	// Destinations: corners one unit a texel, the middle taking the rest.
	check_rectangle(pieces[0].destination, 10.0f, 20.0f, 6.0f, 6.0f);
	check_rectangle(pieces[1].destination, 16.0f, 20.0f, 90.0f, 6.0f);
	check_rectangle(pieces[2].destination, 106.0f, 20.0f, 4.0f, 6.0f);
	check_rectangle(pieces[3].destination, 10.0f, 26.0f, 6.0f, 30.0f);
	check_rectangle(pieces[4].destination, 16.0f, 26.0f, 90.0f, 30.0f);
	check_rectangle(pieces[5].destination, 106.0f, 26.0f, 4.0f, 30.0f);
	check_rectangle(pieces[6].destination, 10.0f, 56.0f, 6.0f, 4.0f);
	check_rectangle(pieces[7].destination, 16.0f, 56.0f, 90.0f, 4.0f);
	check_rectangle(pieces[8].destination, 106.0f, 56.0f, 4.0f, 4.0f);
}

TEST_CASE("scale sizes the corners in world units per texel")
{
	const std::array<SlicePiece, 9> pieces = nine_slice(frame, border,
		RectangleF(0.0f, 0.0f, 100.0f, 100.0f), 2.0f);

	check_rectangle(pieces[0].destination, 0.0f, 0.0f, 12.0f, 12.0f);
	check_rectangle(pieces[4].destination, 12.0f, 12.0f, 80.0f, 80.0f);
	check_rectangle(pieces[8].destination, 92.0f, 92.0f, 8.0f, 8.0f);

	// The source is texels whatever the scale.
	CHECK(pieces[0].source == RectangleI(32, 0, 6, 6));
}

TEST_CASE("a destination smaller than its corners is all corner")
{
	// Five units wide against ten texels of corner: both shrink by half.
	const std::array<SlicePiece, 9> pieces = nine_slice(frame, border,
		RectangleF(0.0f, 0.0f, 5.0f, 50.0f));

	check_rectangle(pieces[0].destination, 0.0f, 0.0f, 3.0f, 6.0f);
	check_rectangle(pieces[1].destination, 3.0f, 0.0f, 0.0f, 6.0f);
	check_rectangle(pieces[2].destination, 3.0f, 0.0f, 2.0f, 6.0f);

	// Nothing is ever turned inside out, at any size down to nothing.
	const std::array<SlicePiece, 9> empty = nine_slice(frame, border,
		RectangleF(7.0f, 7.0f, 0.0f, 0.0f));
	for (const SlicePiece& piece : empty)
	{
		CHECK(piece.destination.width == 0.0f);
		CHECK(piece.destination.height == 0.0f);
		CHECK(piece.destination.x == 7.0f);
		CHECK(piece.destination.y == 7.0f);
	}
}

TEST_CASE("a zero border is a plain stretch in the middle piece")
{
	const std::array<SlicePiece, 9> pieces = nine_slice(frame, SliceBorder{},
		RectangleF(0.0f, 0.0f, 64.0f, 32.0f));

	CHECK(pieces[4].source == frame);
	check_rectangle(pieces[4].destination, 0.0f, 0.0f, 64.0f, 32.0f);
	CHECK(pieces[0].source.width == 0);
	CHECK(pieces[0].destination.width == 0.0f);
}

TEST_CASE("borders that do not fit, and impossible sizes, are refused")
{
	const RectangleF somewhere(0.0f, 0.0f, 10.0f, 10.0f);

	CHECK_THROWS_AS(nine_slice(frame, SliceBorder{ 10, 0, 7, 0 }, somewhere),
		std::invalid_argument);
	CHECK_THROWS_AS(nine_slice(frame, SliceBorder{ 0, 9, 0, 8 }, somewhere),
		std::invalid_argument);
	CHECK_THROWS_AS(nine_slice(frame, SliceBorder{ -1, 0, 0, 0 }, somewhere),
		std::invalid_argument);
	CHECK_THROWS_AS(nine_slice(frame, border,
		RectangleF(0.0f, 0.0f, -1.0f, 10.0f)), std::invalid_argument);
	CHECK_THROWS_AS(nine_slice(frame, border, somewhere, 0.0f),
		std::invalid_argument);

	// Borders that exactly fill the source are a frame with no middle.
	CHECK_NOTHROW(nine_slice(frame, SliceBorder{ 8, 8, 8, 8 }, somewhere));
}

TEST_CASE("a NineSlice keeps its pieces in step with its rectangle")
{
	NineSlice panel("panel", TextureHandle(), frame, border,
		RectangleF(0.0f, 0.0f, 100.0f, 40.0f), Colour(10, 20, 30));

	CHECK(panel.bounds() == RectangleF(0.0f, 0.0f, 100.0f, 40.0f));
	CHECK(panel.colour() == Colour(10, 20, 30));
	check_rectangle(panel.pieces()[4].destination, 6.0f, 6.0f, 90.0f, 30.0f);

	panel.set_rectangle(RectangleF(5.0f, 5.0f, 50.0f, 50.0f));
	CHECK(panel.bounds() == RectangleF(5.0f, 5.0f, 50.0f, 50.0f));
	check_rectangle(panel.pieces()[4].destination, 11.0f, 11.0f, 40.0f, 40.0f);

	// A rectangle the frame cannot take leaves the panel as it was.
	CHECK_THROWS_AS(panel.set_rectangle(RectangleF(0.0f, 0.0f, -5.0f, 5.0f)),
		std::invalid_argument);
	CHECK(panel.bounds() == RectangleF(5.0f, 5.0f, 50.0f, 50.0f));

	panel.set_colour(Colour::white);
	CHECK(panel.colour() == Colour::white);
}

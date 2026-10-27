#include <doctest/doctest.h>

#include "engine/render/colour.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cmath>

using labrador::Colour;
using linesweeper::Kind;
using linesweeper::kind_colour;
using linesweeper::shadow_edge_colour;
using linesweeper::shadow_fill_colour;
using linesweeper::well_colour;

// The landing shadow against the well, computed from the palette alone.
//
// WHAT IS ASSERTED IS WHAT A PLAYER NEEDS, NOT WHAT IT LOOKS LIKE. The
// shadow's colours are a matter for somebody looking at the screen; what must
// not change quietly is that its visible edge stands out from the empty well
// at WCAG 1.4.11's 3:1 for every kind, and that it is still not a piece. Both
// are arithmetic on palette.h's constants through the blend equation the
// board draws with, so a palette edit that breaks either fails here rather
// than on the hosted demo. It does not prove the shadow is drawn this way:
// board_view.cpp draws it, and that is checked by looking at the captures.
namespace
{
	constexpr std::array<Kind, 7> kinds = { Kind::i, Kind::j, Kind::l,
		Kind::o, Kind::s, Kind::t, Kind::z };

	// `dst = src.rgb + dst.rgb * (1 - a)`: palette.h's premultiplied blend,
	// drawing `source` over an opaque `destination`.
	Colour over(const Colour& source, const Colour& destination)
	{
		const float keep = 1.0f - source.a;

		return Colour(source.r + destination.r * keep,
			source.g + destination.g * keep, source.b + destination.b * keep);
	}

	// WCAG 2.2's relative luminance of an sRGB channel and of a colour.
	double linear(float channel)
	{
		const double value = static_cast<double>(channel);

		return value <= 0.04045 ? value / 12.92
								: std::pow((value + 0.055) / 1.055, 2.4);
	}

	double luminance(const Colour& colour)
	{
		return 0.2126 * linear(colour.r) + 0.7152 * linear(colour.g) +
			0.0722 * linear(colour.b);
	}

	double contrast(const Colour& first, const Colour& second)
	{
		const double a = luminance(first);
		const double b = luminance(second);
		const double lighter = a > b ? a : b;
		const double darker = a > b ? b : a;

		return (lighter + 0.05) / (darker + 0.05);
	}
}

TEST_CASE("every landing shadow's edge stands out from the well at 3:1")
{
	for (const Kind kind : kinds)
	{
		const Colour edge = over(shadow_edge_colour(kind), well_colour);

		CAPTURE(static_cast<int>(kind));
		CHECK(contrast(edge, well_colour) >= 3.0);
	}
}

TEST_CASE("the landing shadow is not a piece")
{
	for (const Kind kind : kinds)
	{
		CAPTURE(static_cast<int>(kind));

		// Its inside lets the well through, where a piece is opaque.
		CHECK(shadow_fill_colour(kind).a < 1.0f);
		CHECK(kind_colour(kind).a == doctest::Approx(1.0f));

		// And the inside is darker than the piece of the same kind, so the
		// two are told apart by more than their edges.
		const Colour inside = over(shadow_fill_colour(kind), well_colour);
		const Colour piece = over(kind_colour(kind), well_colour);

		CHECK(luminance(inside) < luminance(piece));
	}
}

// The well itself has to be what the board draws as empty: opaque, so the
// contrast above is against a colour and not against whatever is behind it.
TEST_CASE("the well is opaque")
{
	CHECK(well_colour.a == doctest::Approx(1.0f));
}

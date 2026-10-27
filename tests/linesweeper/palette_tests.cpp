#include <doctest/doctest.h>

#include "engine/render/colour.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

using labrador::Colour;
using linesweeper::Kind;
using linesweeper::Palette;
using linesweeper::grid_colour;
using linesweeper::kind_colour;
using linesweeper::next_palette;
using linesweeper::palette_label;
using linesweeper::palette_name;
using linesweeper::palettes;
using linesweeper::previous_palette;
using linesweeper::shadow_edge_colour;
using linesweeper::shadow_fill_colour;
using linesweeper::well_colour;

// The palettes against the well, computed from palette.h alone.
//
// WHAT IS ASSERTED IS WHAT A PLAYER NEEDS, NOT WHAT IT LOOKS LIKE. The
// colours are a matter for somebody looking at the screen; what must not
// change quietly is that the shadow's visible edge stands out from the empty
// well and from the grid line over it at WCAG 1.4.11's 3:1 for every kind in
// every palette, that the shadow is still not a piece, that each palette tells
// its seven kinds apart at all, and that no colour-vision palette brings back
// the saturated red WCAG 2.3.1 counts. All of it is arithmetic on palette.h's
// constants through the blend equation the board draws with, so a palette
// edit that breaks one fails here rather than on the hosted demo. That the
// board draws with these functions is board_view_tests.cpp's, under the null
// renderer; how well a palette serves the group it is for is a simulation the
// README records and no test can hold.
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

	// tools/flash_check/'s saturated red: red's share of the linear channels.
	double red_share(const Colour& colour)
	{
		const double red = linear(colour.r);
		const double total = red + linear(colour.g) + linear(colour.b);

		return total > 0.0 ? red / total : 0.0;
	}

	int index_of(Palette palette)
	{
		return static_cast<int>(palette);
	}
}

TEST_CASE("every landing shadow's edge stands out from the well at 3:1")
{
	for (const Palette palette : palettes)
	{
		for (const Kind kind : kinds)
		{
			const Colour edge = over(shadow_edge_colour(kind, palette),
				well_colour);

			CAPTURE(index_of(palette));
			CAPTURE(static_cast<int>(kind));
			CHECK(contrast(edge, well_colour) >= 3.0);
		}
	}
}

// The edge runs along the one-pixel grid that keeps the well reading as ten
// columns, so it has to stand out from that line as well as from the empty
// cell beside it. The lowest is the standard palette's J, at about 3.26.
TEST_CASE("every landing shadow's edge stands out from the grid line at 3:1")
{
	const Colour line = over(grid_colour, well_colour);

	for (const Palette palette : palettes)
	{
		for (const Kind kind : kinds)
		{
			const Colour edge = over(shadow_edge_colour(kind, palette),
				well_colour);

			CAPTURE(index_of(palette));
			CAPTURE(static_cast<int>(kind));
			CHECK(contrast(edge, line) >= 3.0);
		}
	}
}

TEST_CASE("the landing shadow is not a piece")
{
	for (const Palette palette : palettes)
	{
		for (const Kind kind : kinds)
		{
			CAPTURE(index_of(palette));
			CAPTURE(static_cast<int>(kind));

			// Its inside lets the well through, where a piece is opaque.
			CHECK(shadow_fill_colour(kind, palette).a < 1.0f);
			CHECK(kind_colour(kind, palette).a == doctest::Approx(1.0f));

			// And the inside is darker than the piece of the same kind, so
			// the two are told apart by more than their edges.
			const Colour inside = over(shadow_fill_colour(kind, palette),
				well_colour);
			const Colour piece = over(kind_colour(kind, palette), well_colour);

			CHECK(luminance(inside) < luminance(piece));
		}
	}
}

// The well itself has to be what the board draws as empty: opaque, so the
// contrast above is against a colour and not against whatever is behind it.
TEST_CASE("the well is opaque")
{
	CHECK(well_colour.a == doctest::Approx(1.0f));
}

// The default is what every website capture and the hosted demo were drawn
// in, so it is pinned to the channel: a change here moves every image.
TEST_CASE("the standard palette is the genre's seven, unchanged")
{
	constexpr std::array<std::array<int, 3>, 7> standard = { {
		{ 0, 240, 240 },
		{ 0, 96, 240 },
		{ 240, 152, 0 },
		{ 240, 208, 0 },
		{ 0, 208, 80 },
		{ 168, 48, 240 },
		{ 240, 48, 48 },
	} };

	for (std::size_t index = 0; index < kinds.size(); ++index)
	{
		const Colour colour = kind_colour(kinds[index], Palette::standard);

		CAPTURE(index);
		CHECK(colour == Colour(standard[index][0], standard[index][1],
			standard[index][2]));
	}

	CHECK(linesweeper::standard_palette == Palette::standard);
}

TEST_CASE("each palette's seven are opaque and distinct, and empty is clear")
{
	for (const Palette palette : palettes)
	{
		CAPTURE(index_of(palette));

		for (std::size_t first = 0; first < kinds.size(); ++first)
		{
			CAPTURE(first);
			CHECK(kind_colour(kinds[first], palette).a == 1.0f);

			for (std::size_t second = first + 1; second < kinds.size();
				++second)
			{
				CAPTURE(second);
				CHECK(kind_colour(kinds[first], palette) !=
					kind_colour(kinds[second], palette));
			}
		}

		CHECK(kind_colour(Kind::none, palette).a == 0.0f);
		CHECK(kind_colour(static_cast<Kind>(8), palette).a == 0.0f);
	}
}

// WCAG 2.3.1's red flash, as tools/flash_check/ measures it: a pixel whose
// linear red is at least 0.8 of its channels' sum. The standard palette's Z
// is 0.94 and is left as the genre has it - the flash check measures its
// bursts as they are - but a palette chosen for players who see red poorly or
// differently has no reason to carry one.
TEST_CASE("no colour-vision palette has a saturated red")
{
	for (const Palette palette : palettes)
	{
		if (palette == Palette::standard)
		{
			continue;
		}

		for (const Kind kind : kinds)
		{
			CAPTURE(index_of(palette));
			CAPTURE(static_cast<int>(kind));
			CHECK(red_share(kind_colour(kind, palette)) < 0.8);
		}
	}
}

TEST_CASE("next and previous visit every palette and undo each other")
{
	Palette forward = Palette::standard;
	Palette backward = Palette::standard;
	std::array<int, 4> seen_forward = {};
	std::array<int, 4> seen_backward = {};

	for (std::size_t step = 0; step < palettes.size(); ++step)
	{
		++seen_forward[static_cast<std::size_t>(forward)];
		++seen_backward[static_cast<std::size_t>(backward)];

		CHECK(previous_palette(next_palette(forward)) == forward);
		CHECK(next_palette(previous_palette(forward)) == forward);

		forward = next_palette(forward);
		backward = previous_palette(backward);
	}

	// Round to the start in both directions, each palette once on the way.
	CHECK(forward == Palette::standard);
	CHECK(backward == Palette::standard);
	for (std::size_t index = 0; index < palettes.size(); ++index)
	{
		CAPTURE(index);
		CHECK(seen_forward[index] == 1);
		CHECK(seen_backward[index] == 1);
	}

	CHECK(next_palette(Palette::standard) == Palette::protan);
	CHECK(previous_palette(Palette::standard) == Palette::tritan);
}

// The backlog's names for the three, word for word, which is what the console
// line a page test reads is built from.
TEST_CASE("the palettes are labelled with the deficiencies' full names")
{
	CHECK(std::string(palette_label(Palette::standard)) == "Standard");
	CHECK(std::string(palette_label(Palette::protan)) ==
		"Protanomaly / Protanopia");
	CHECK(std::string(palette_label(Palette::deutan)) ==
		"Deuteranomaly / Deuteranopia");
	CHECK(std::string(palette_label(Palette::tritan)) ==
		"Tritanomaly / Tritanopia");
}

// The pause menu shows the same names in capitals. The menu's text is not
// read by any page test, so a typo or a swapped name would otherwise ship.
TEST_CASE("the pause menu names each palette as its label, in capitals")
{
	for (const Palette palette : palettes)
	{
		const std::string label = palette_label(palette);
		const std::wstring name = palette_name(palette);

		CAPTURE(label);
		REQUIRE(name.size() == label.size());

		for (std::size_t index = 0; index < label.size(); ++index)
		{
			const char letter = label[index];
			const char upper = letter >= 'a' && letter <= 'z' ?
				static_cast<char>(letter - 'a' + 'A') : letter;

			CHECK(name[index] == static_cast<wchar_t>(upper));
		}
	}
}

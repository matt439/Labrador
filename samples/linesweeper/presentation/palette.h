#pragma once

#include "engine/render/colour.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cstddef>
#include <cstdint>

// What a cell looks like, and nothing else.
//
// THIS IS THE WHOLE OF WHAT THE PRESENTATION KNOWS ABOUT A PIECE. The rules
// store a byte between one and seven in every filled cell and have no opinion
// about it at all (world.h): a colour is not a rule, and a recording replays
// identically whatever this file says. Every colour a piece can be drawn in is
// in this file, in four palettes.
//
// WHICH PALETTE IS SHOWING IS THE PRESENTATION'S, NEVER THE MATCH'S. PlayState
// owns the selection beside its World and lends it to the board and the
// particle field as a const pointer; the pause menu is the one place that
// writes it. It is not in World, not in the recording and not an input to
// tick(), so changing it mid-match changes what the next frame looks like and
// nothing the next tick does.
//
// A header with no type in it, named for what it computes, which CONVENTIONS
// allows and calls the exception rather than a second pattern. It computes one
// thing.
namespace linesweeper
{
	// The pieces' colour sets. `standard` is the genre's guideline seven,
	// which is what anybody who has played the genre expects each shape to
	// be. Each of the other three is chosen for one group of colour-vision
	// deficiency and for that group only: a palette that separates the seven
	// for one group can collapse a pair for another (README, Colour-vision
	// palettes, has the measurements and their limits).
	enum class Palette : std::uint8_t
	{
		standard,
		protan,
		deutan,
		tritan,
	};

	// Every palette, in the order the pause menu steps through them.
	inline constexpr std::array<Palette, 4> palettes = { Palette::standard,
		Palette::protan, Palette::deutan, Palette::tritan };

	// What a match starts in, and what anything that draws the board without
	// a player to ask - the frame bench, the pacing fixture - lends.
	inline constexpr Palette standard_palette = Palette::standard;

	// The next and the previous palette in `palettes`, wrapping at both ends.
	constexpr Palette next_palette(Palette palette)
	{
		return palettes[(static_cast<std::size_t>(palette) + 1) %
			palettes.size()];
	}

	constexpr Palette previous_palette(Palette palette)
	{
		return palettes[(static_cast<std::size_t>(palette) + palettes.size() -
			1) % palettes.size()];
	}

	// What the pause menu calls a palette: in capitals, because every word
	// on that screen is, and naming both the anomalous and the dichromatic
	// form, because a player knows which one they have and should not have to
	// guess that one palette is for both.
	constexpr const wchar_t* palette_name(Palette palette)
	{
		switch (palette)
		{
		case Palette::protan:
			return L"PROTANOMALY / PROTANOPIA";
		case Palette::deutan:
			return L"DEUTERANOMALY / DEUTERANOPIA";
		case Palette::tritan:
			return L"TRITANOMALY / TRITANOPIA";
		case Palette::standard:
		default:
			return L"STANDARD";
		}
	}

	// The same names in ordinary case, for a console line and a command
	// line: the browser build logs the selection with it, and a page test
	// reads that line.
	constexpr const char* palette_label(Palette palette)
	{
		switch (palette)
		{
		case Palette::protan:
			return "Protanomaly / Protanopia";
		case Palette::deutan:
			return "Deuteranomaly / Deuteranopia";
		case Palette::tritan:
			return "Tritanomaly / Tritanopia";
		case Palette::standard:
		default:
			return "Standard";
		}
	}

	namespace detail
	{
		// [palette][kind - 1], in Kind's order: I, J, L, O, S, T, Z.
		//
		// The three colour-vision rows were chosen by simulation (README,
		// Colour-vision palettes): every pair of the seven kept as far apart
		// as a search could get them under that row's own deficiency, every
		// kind at least 3:1 against the well and against the grid line drawn
		// over it, and no kind a saturated red by WCAG 2.3.1's measure.
		// palette_tests.cpp holds the last three; the first is a development
		// aid and not a test.
		inline constexpr std::array<std::array<labrador::Colour, kind_count>,
			palettes.size()> kind_colours = { {
			{ {
				labrador::Colour(0, 240, 240),
				labrador::Colour(0, 96, 240),
				labrador::Colour(240, 152, 0),
				labrador::Colour(240, 208, 0),
				labrador::Colour(0, 208, 80),
				labrador::Colour(168, 48, 240),
				labrador::Colour(240, 48, 48),
			} },
			{ {
				labrador::Colour(80, 255, 220),
				labrador::Colour(0, 116, 255),
				labrador::Colour(212, 132, 8),
				labrador::Colour(244, 240, 0),
				labrador::Colour(0, 132, 120),
				labrador::Colour(204, 24, 124),
				labrador::Colour(244, 180, 224),
			} },
			{ {
				labrador::Colour(16, 252, 212),
				labrador::Colour(40, 116, 228),
				labrador::Colour(196, 144, 96),
				labrador::Colour(255, 236, 0),
				labrador::Colour(0, 126, 0),
				labrador::Colour(244, 140, 255),
				labrador::Colour(188, 64, 120),
			} },
			{ {
				labrador::Colour(24, 248, 228),
				labrador::Colour(0, 140, 196),
				labrador::Colour(188, 100, 56),
				labrador::Colour(204, 224, 28),
				labrador::Colour(104, 116, 44),
				labrador::Colour(148, 40, 255),
				labrador::Colour(244, 180, 220),
			} },
		} };
	}

	// A filled cell of `kind` in `palette`. An empty cell, or a byte outside
	// the seven, is transparent.
	//
	// No default palette, deliberately: every caller that draws a piece has
	// to say which palette it is drawing in, so none can quietly stay in the
	// standard one.
	constexpr labrador::Colour kind_colour(Kind kind, Palette palette)
	{
		const std::size_t index = static_cast<std::size_t>(kind);

		if (index == 0 || index > static_cast<std::size_t>(kind_count))
		{
			return labrador::Colour(0, 0, 0, 0);
		}

		return detail::kind_colours[static_cast<std::size_t>(palette)]
			[index - 1];
	}

	// THE BLEND STATE IS PREMULTIPLIED ALPHA, and this function is what that
	// costs a caller.
	//
	// DirectXTK substitutes CommonStates::AlphaBlend() for the null blend
	// state every batch opens with, and that is BLEND_ONE against
	// BLEND_INV_SRC_ALPHA: the equation is `dst = src.rgb + dst.rgb * (1 - a)`,
	// so the source's colour channels are expected to have been multiplied by
	// its alpha already. A tint of (r, g, b, 0.25) therefore does not fade the
	// quad to a quarter - it adds nearly all of the colour and lets nearly all
	// of the background through, which reads as a glow.
	//
	// Which is a feature elsewhere and a bug here. The inside of the shadow
	// under a falling piece wants to be dimmer than the piece, so it
	// multiplies. The sample's README argues that the same one state expresses
	// opaque drawing, a fade and additive glow; this is the line where that
	// stops being an argument and starts being two multiplications.
	constexpr labrador::Colour faded(const labrador::Colour& colour,
		float alpha)
	{
		return labrador::Colour(colour.r * alpha, colour.g * alpha,
			colour.b * alpha, alpha);
	}

	// THE OTHER HALF OF THE SAME EQUATION, and the one the README spent a
	// section predicting would need an atlas.
	//
	// faded() above scales the colour AND the alpha, which under
	// `dst = src.rgb + dst.rgb * (1 - a)` is ordinary alpha blending: some of
	// the source, the rest of the background. Hold the alpha at zero instead
	// and the second term is untouched - the source simply adds. That is
	// additive glow, out of the one blend state every batch already opens
	// with, with no second state, no seam change and no atlas.
	//
	// `brightness` is therefore a multiplier on what the background already
	// has rather than a mix against it, so overlapping draws saturate towards
	// white instead of averaging. Ten thousand of them doing that is what a
	// burst looks like (presentation/particles.cpp).
	//
	// A tint and not a texture, which is the part worth recording: the sample
	// predicted the glow would arrive as an atlas of soft radial dots and it
	// did not. A shrinking quad of pure addition reads as a spark at four
	// pixels across, and the content stays one white texel.
	constexpr labrador::Colour glowing(const labrador::Colour& colour,
		float brightness)
	{
		return labrador::Colour(colour.r * brightness, colour.g * brightness,
			colour.b * brightness, 0.0f);
	}

	// What an empty cell looks like: the colour everything in the well is
	// drawn over, and so the colour every contrast in it is measured against.
	// The same in every palette.
	inline constexpr labrador::Colour well_colour(12, 14, 18);

	// The one-pixel grid that keeps an empty well reading as ten columns,
	// drawn over the well. Here rather than beside the board that draws it
	// because a shadow's edge runs along it, so palette_tests.cpp measures
	// every edge against it as well as against the well.
	//
	// Spelt out rather than faded from Colour::white, which is a
	// `static const` object in another translation unit: a namespace-scope
	// constant built from one is a cross-unit initialisation order the
	// standard does not fix.
	inline constexpr labrador::Colour grid_colour =
		faded(labrador::Colour(1.0f, 1.0f, 1.0f), 0.05f);

	// THE LANDING SHADOW IS AN OUTLINE, because a fill alone cannot be both
	// visible and unlike a piece.
	//
	// The shadow must stand out from the empty well at WCAG's 3:1 for a
	// graphic the player needs, for all seven kinds. A faded fill fails that
	// for the dark ones: standard J at full strength is only about 3.6:1
	// against the well, so a fill that reached 3:1 would keep about 88% of the
	// piece and read as a second piece. So the visible edge is the piece's own
	// colour at full strength, which is as far from the well as that kind can
	// get, and what stops it reading as a piece is the inside: a fill that
	// keeps only a little of the colour. palette_tests.cpp checks the edge of
	// every kind in every palette against the well and the grid line through
	// the blend equation, so a palette edit that drops one below 3:1 fails a
	// test rather than a player.
	inline constexpr float shadow_fill_alpha = 0.28f;

	constexpr labrador::Colour shadow_edge_colour(Kind kind, Palette palette)
	{
		return kind_colour(kind, palette);
	}

	constexpr labrador::Colour shadow_fill_colour(Kind kind, Palette palette)
	{
		return faded(kind_colour(kind, palette), shadow_fill_alpha);
	}
}

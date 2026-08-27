#pragma once

#include "engine/render/colour.h"
#include "samples/linesweeper/rules/world.h"

// What a cell looks like, and nothing else.
//
// THIS IS THE WHOLE OF WHAT THE PRESENTATION KNOWS ABOUT A PIECE. The rules
// store a byte between one and seven in every filled cell and have no opinion
// about it at all (world.h): a colour is not a rule, a recording replays
// identically whatever this file says, and the day this sample grows a
// colour-blind palette it is this file that gains it and nothing else that
// changes.
//
// A header with no type in it, named for what it computes, which CONVENTIONS
// allows and calls the exception rather than a second pattern. It computes one
// thing.
namespace linesweeper
{
	// The guideline's seven, which is what anybody who has played the genre
	// expects each shape to be.
	constexpr labrador::Colour kind_colour(Kind kind)
	{
		switch (kind)
		{
		case Kind::i:
			return labrador::Colour(0, 240, 240);
		case Kind::j:
			return labrador::Colour(0, 96, 240);
		case Kind::l:
			return labrador::Colour(240, 152, 0);
		case Kind::o:
			return labrador::Colour(240, 208, 0);
		case Kind::s:
			return labrador::Colour(0, 208, 80);
		case Kind::t:
			return labrador::Colour(168, 48, 240);
		case Kind::z:
			return labrador::Colour(240, 48, 48);
		case Kind::none:
		default:
			return labrador::Colour(0, 0, 0, 0);
		}
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
	// Which is a feature elsewhere and a bug here. The shadow under a falling
	// piece wants to be dimmer than the piece, so it multiplies. The sample's
	// README argues that the same one state expresses opaque drawing, a fade
	// and additive glow; this is the line where that stops being an argument
	// and starts being two multiplications.
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
}

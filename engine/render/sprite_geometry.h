#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_vertex.h"

// Where a sprite's four corners go, and what they sample.
//
// THIS FILE IS THE PIXEL CONTRACT. Every term RenderPixelTests pins is decided
// here and nowhere else: that the destination is in pixels with y running down,
// that texel (0,0) lands at the destination's top left, that a flip mirrors the
// texture and not the rectangle, that the source rectangle is in texels, that
// the tint multiplies, that a fractional destination truncates with an
// exclusive right edge, and that the origin is measured in unscaled source
// texels. They were DirectXTK's, inside SpriteBatch::Impl::RenderSprite, where
// nothing outside that library could read them and no test could reach them.
//
// A BACKEND DOES NOT DO THIS ARITHMETIC, WHICH IS THE WHOLE POINT. What a
// backend receives is four SpriteVertex in view pixels; what it owes is a
// buffer, a shader that multiplies by two constants, and the state that makes
// the blend premultiplied. Two backends cannot disagree about where a sprite
// went, because only one of them decides.
//
// THE CORNER ORDER IS PART OF THE CONTRACT: 0 is the destination's top left, 1
// its top right, 2 its bottom left, 3 its bottom right. A backend's index
// buffer is built from that and from nothing else, so the two triangles are
// (0,1,2) and (1,3,2).

namespace artattack
{
	// Fills `corners[4]` for a sprite drawn into `destination`.
	//
	// `destination` is in view pixels and `source` in texels of a texture
	// `texture_size` texels across. `origin` is in UNSCALED SOURCE TEXELS - so
	// an 8x8 destination over a 2x2 source scales an origin of two texels into
	// a shift of eight pixels, which is the term the seam states and the one
	// most often assumed to be destination pixels instead.
	//
	// THE DESTINATION TRUNCATES. Each of its four edges is taken to a whole
	// pixel by dropping the fraction, and the size is the difference of the
	// truncated edges rather than the truncation of the size - which is not the
	// same number, and is why this takes a rectangle rather than a position and
	// a size. A right edge of 18.9 becomes 18 and is exclusive.
	void build_sprite_quad(const mattmath::RectangleF& destination,
		const mattmath::RectangleI& source,
		const mattmath::Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const mattmath::Vector2F& origin,
		SpriteFlip flip,
		SpriteVertex* corners);

	// The same, for a sprite placed by a position and a uniform scale over its
	// source rather than by a rectangle.
	//
	// NOTHING TRUNCATES HERE, and that is the difference that matters. Text is
	// laid out by this: a pen advance is fractional in most fonts, and rounding
	// each glyph to a whole pixel would make a line of text drift against its
	// own measurement. There is no flip because no caller flips text, and
	// flipped text is named in RenderPixelTests as something nothing pins.
	void build_scaled_quad(const mattmath::Vector2F& position,
		float scale,
		const mattmath::RectangleI& source,
		const mattmath::Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const mattmath::Vector2F& origin,
		SpriteVertex* corners);
}

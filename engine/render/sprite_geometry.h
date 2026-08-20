#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
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
// buffer, a shader that multiplies each vertex by one four-float constant and
// the sampled texel by the vertex's own colour, and the state that makes the
// blend premultiplied. Four backends cannot disagree about where a sprite went,
// because none of them decides.
//
// THE ONE TERM A BACKEND STILL OWNS is where the pane itself sits in the thing
// being drawn into, because that is the one question the answer to which is not
// the same shape on both APIs: D3D11 measures a viewport down from the render
// target's top left and needs no height at all, GL measures up from the bottom
// and so has to subtract from one. It held a cached copy of that height and it
// was wrong for the whole of every drag-resize; it reads the window now
// (engine/render/gl/backend.h, Impl::drawable_size), which is what makes the
// sentence above true rather than nearly true.
//
// THE CORNER ORDER IS PART OF THE CONTRACT: 0 is the destination's top left, 1
// its top right, 2 its bottom left, 3 its bottom right. A backend's index
// buffer is built from that and from nothing else, so the two triangles are
// (0,1,2) and (1,3,2).

namespace labrador
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

	// One glyph of a line of text, at the pen the walk (font.h) reported.
	//
	// `position`, `scale`, `tint` and `rotation` are the whole string's, and
	// `origin` is the caller's origin for the string - all four the same for
	// every glyph in it. What varies per glyph is `glyph` and `pen`.
	//
	// THIS IS THE TERM THE WALK DELIBERATELY DOES NOT CARRY. for_each_glyph
	// reports pen.y as the TOP OF THE LINE and leaves the glyph's own y_offset
	// out, because measurement wants the line and drawing wants the bearing;
	// this is where drawing adds it back. It lived in each backend's
	// draw_text until it was written here, which meant three copies of the one
	// line that decides how high a glyph sits, none of them reachable by a
	// test - deleting it from any single copy left every configuration green,
	// because the pixel contract's text cases compare two glyphs drawn by the
	// same code and a term applied to both cancels out of the comparison.
	//
	// The pen subtracts because it moves the glyph and an origin moves the
	// string: shifting the origin left by the pen puts the glyph right of the
	// string's position by exactly the pen. Both are in unscaled source
	// texels, which is what build_scaled_quad already means by an origin, so
	// the two compose without a conversion.
	void build_glyph_quad(const mattmath::Vector2F& position,
		float scale,
		const Glyph& glyph,
		const mattmath::Vector2F& pen,
		const mattmath::Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const mattmath::Vector2F& origin,
		SpriteVertex* corners);
}

#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_vertex.h"

#include <string>

// Where a sprite's four corners go, and what they sample.
//
// THIS FILE IS THE PIXEL CONTRACT. Every term RenderPixelTests pins is decided
// here and nowhere else: that the destination is in pixels with y running down,
// that texel (0,0) lands at the destination's top left, that a flip mirrors the
// texture and not the rectangle, that the source rectangle is in texels, that
// the tint multiplies, that a fractional destination truncates with an
// exclusive right edge, and that the origin is measured in unscaled source
// texels.
//
// A BACKEND DOES NOT DO THIS ARITHMETIC, WHICH IS THE WHOLE POINT. What a
// backend receives is four SpriteVertex in view pixels; what it owes is a
// buffer, a shader that multiplies each vertex by one four-float constant and
// the sampled texel by the vertex's own colour, and the state that makes the
// blend premultiplied. No two backends can disagree about where a sprite went,
// because none of them decides - the null backend included, which runs this
// file too, and that is what makes its recording worth asserting on.
//
// THE ONE TERM A BACKEND STILL OWNS is where the pane itself sits in the thing
// being drawn into, because that is the one question whose answer is not the
// same shape on every API behind this seam. Direct3D measures a viewport down
// from the render target's top left and needs no height at all. GL measures up
// from the bottom and so has to subtract from one, which it reads from the
// window rather than from a cached copy, because a cached copy is wrong for the
// whole of every drag-resize (engine/render/gl/backend.h, Impl::drawable_size).
// Vulkan hands the rasteriser a viewport whose origin is the pane's BOTTOM edge
// and whose height is negative, which inverts y for the whole pipeline and
// makes the flip pane-local rather than buffer-relative - so the buffer's
// height never enters it (engine/render/vulkan/renderer.cpp). Each keeps the
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
	void build_sprite_quad(const RectangleF& destination,
		const RectangleI& source,
		const Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
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
	//
	// SO THIS IS WHERE THE CONTRACT STOPS DECIDING A PIXEL. A position half a
	// pixel off a whole one puts every pixel centre on a texel edge, and the
	// texel the point filter picks there is the rasteriser's rounding of an
	// interpolated coordinate rather than anything written here. The caller
	// avoids it, as renderer.h's draw_text says; a nudge applied here would
	// move every fractional position to fix one of them.
	void build_scaled_quad(const Vector2F& position,
		float scale,
		const RectangleI& source,
		const Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
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
	// this is where drawing adds it back.
	//
	// The pen subtracts because it moves the glyph and an origin moves the
	// string: shifting the origin left by the pen puts the glyph right of the
	// string's position by exactly the pen. Both are in unscaled source
	// texels, which is what build_scaled_quad already means by an origin, so
	// the two compose without a conversion.
	void build_glyph_quad(const Vector2F& position,
		float scale,
		const Glyph& glyph,
		const Vector2F& pen,
		const Vector2F& texture_size,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteVertex* corners);

	// THE AXIS-ALIGNED BOX AROUND THE QUAD build_sprite_quad WOULD BUILD from
	// the same four arguments: the same truncated edges, the same origin
	// shift, the same turn about the destination's top left. Tight, not
	// merely conservative - it is the bounding box of the four corners.
	//
	// This is a view-space measurement, not a world-space cull bound.
	// sprite_world_bounds accounts for the camera-dependent quantization. It
	// lives here because it has to agree with build_sprite_quad to the term.
	// The destination rectangle is where a sprite lands only when its origin
	// is zero and its rotation is zero. An authored frame origin shifts the
	// sprite by that many texels' worth of destination, and a rotation turns
	// it about its top left, so a sprite at x=100 with an origin of its own
	// width draws across x=80..100 - and a cull against x=100..120 would drop
	// it from any view that ends at x=90 while it is plainly inside, on every
	// backend at once, because none of them decides this.
	//
	// `origin` is the WHOLE origin - the frame's authored one plus the
	// caller's, summed the way SpriteSheet::draw sums them - in unscaled
	// source texels, exactly as build_sprite_quad takes it.
	RectangleF sprite_quad_bounds(
		const RectangleF& destination,
		const RectangleI& source,
		float rotation,
		const Vector2F& origin);

	// World geometry before view-space quantization, expanded by a bound on
	// its error at nonnegative world units per pixel (zero for layout).
	// Each truncated edge moves by less than one pixel; a size moves by less
	// than two. Rotation mixes those errors and the source-relative pivot
	// scales them, including origins outside the source rectangle.
	RectangleF sprite_world_bounds(
		const RectangleF& destination,
		const RectangleI& source, float rotation,
		const Vector2F& origin, float units_per_pixel = 0.0f);

	// The same, for a string laid out by build_glyph_quad: `measured_size`
	// is the unscaled size of its enclosing local rectangle, and
	// the box is that size, scaled, sitting `origin` scaled texels up and
	// left of `position`, turned about `position`. Font::measure reports a
	// layout extent; callers enclosing drawn glyph cells as well must include
	// overhanging bearings and subtract the local rectangle's position from
	// the text's original origin. The transform then keeps that origin fixed.
	RectangleF text_quad_bounds(const Vector2F& position,
		float scale,
		const Vector2F& measured_size,
		float rotation,
		const Vector2F& origin);

	// A texture handle a backend has resolved: whatever its view's draw()
	// records, and the size in texels that the quad arithmetic needs.
	// `Texture` is a pointer, a reference or a handle, as the backend's
	// draw() takes it.
	template <typename Texture>
	struct ResolvedTexture
	{
		Texture texture;
		Vector2F size;
	};

	// DrawList::draw_sprite once the backend has resolved the handle. The
	// view's camera maps `destination`, build_sprite_quad builds the corners,
	// and the view records them. `View` is the backend's DrawList::View; it
	// has a `camera` and a `draw(texture, corners)`. A template rather than a
	// virtual call, because it runs once per sprite (T8).
	template <typename View, typename Texture>
	void record_sprite(View& view,
		const ResolvedTexture<Texture>& texture,
		const RectangleI& source,
		const RectangleF& destination,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip)
	{
		SpriteVertex corners[4];
		build_sprite_quad(view.camera.calculate_view_rectangle(destination),
			source, texture.size, tint, rotation, origin, flip, corners);
		view.draw(texture.texture, corners);
	}

	// DrawList::draw_text once the backend has resolved the font's atlas. A
	// glyph is a sprite: the font walks the text, build_glyph_quad places
	// each glyph, and the view records it as record_sprite's view does.
	template <typename View, typename Texture>
	void record_text(View& view,
		const Font& font,
		const ResolvedTexture<Texture>& atlas,
		const std::wstring& text,
		const Vector2F& position,
		const Colour& tint,
		float scale,
		float rotation,
		const Vector2F& origin)
	{
		const Vector2F screen_position =
			view.camera.calculate_view_position(position);
		const float screen_scale = view.camera.calculate_view_scale(scale);

		font.for_each_glyph(text,
			[&](const Glyph& glyph, const Vector2F& pen)
			{
				SpriteVertex corners[4];
				build_glyph_quad(screen_position, screen_scale, glyph, pen,
					atlas.size, tint, rotation, origin, corners);
				view.draw(atlas.texture, corners);
			});
	}
}

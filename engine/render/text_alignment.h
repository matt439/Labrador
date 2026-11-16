#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

namespace labrador
{
	// Which matching point of the measured text layout and anchor to align.
	enum class TextAlignment
	{
		top_left,
		top_centre,
		top_right,
		centre_left,
		centre,
		centre_right,
		bottom_left,
		bottom_centre,
		bottom_right,
	};

	enum class TextSnapping
	{
		none,
		floor,
	};

	// The position for unrotated, zero-origin DrawList::draw_text: align the
	// measured layout rectangle with the chosen point of `anchor`. Use
	// Font::measure or RenderResources::measure_text for measured_size; glyph
	// bearings may extend the ink outside that layout. Multiline text aligns
	// as one measured block, without changing the lines' individual margins.
	//
	// `scale` multiplies the measured size. Pass the draw's scale for an
	// unscaled measurement, or one for an already-scaled size. Measured and
	// anchor sizes must be finite and nonnegative; zero size is supported.
	// Anchor positions must be finite and scale finite and positive. This
	// helper does not place mirrored or rotated text.
	//
	// none preserves fractional coordinates. floor rounds both final position
	// coordinates downward in the anchor's coordinate space, including below
	// zero. Those units are pixels only when the caller's space maps 1:1 to
	// view pixels; a camera or fractional scale can still put glyphs between
	// pixels. Choosing snapping does not change the renderer's drawing policy.
	//
	// Throws std::invalid_argument for invalid inputs, enumerators or a
	// calculated size/position that is not finite. For valid inputs it
	// measures and allocates nothing; it stores no state.
	Vector2F aligned_text_position(const Vector2F& measured_size,
		const RectangleF& anchor, TextAlignment alignment, TextSnapping snapping,
		float scale = 1.0f);

	// A point anchor is the same operation with a zero-sized anchor rectangle.
	// The returned position follows the same contract and snapping choice.
	Vector2F aligned_text_position(const Vector2F& measured_size,
		const Vector2F& anchor, TextAlignment alignment, TextSnapping snapping,
		float scale = 1.0f);
}

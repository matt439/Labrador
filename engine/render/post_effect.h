#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <array>
#include <optional>

// The post stage's materials and their values (docs/effects.md#5). A chain is
// a list of PostEffect, set on the Renderer outside a frame; each effect's
// values are set with Renderer::set_post_values.

namespace labrador
{
	enum class PostEffect
	{
		// The scene, texel for texel: a frame through it is the frame without
		// it, byte for byte.
		identity,

		// A CRT monitor: curvature, scanlines that widen with brightness, an
		// aperture-grille mask, a vignette and colour fringing, inside one
		// screen region. Its values come from crt_values.
		crt,
	};

	// The number of PostEffect values, for a table indexed by one.
	inline constexpr int post_effect_count = 2;

	// The frame's material_values (engine/render/post_frame.hlsli) as the
	// caller fills them: eight groups of four floats, read by a material as
	// material_value(0) to material_value(7).
	struct PostValues
	{
		std::array<float, 32> values{};
	};

	// Every strength defaults to zero, and zero turns that term off; with
	// every term off the CRT draws the scene unchanged.
	struct CrtSettings
	{
		// Back-buffer pixels. Outside it, pixels pass through untouched.
		RectangleF screen;

		// 0 is flat. The picture's corners leave the tube as it grows.
		float curvature = 0.0f;

		// Scanlines across the screen's height, so their number stays fixed
		// as the screen grows or shrinks. Spaced evenly at the centre;
		// curvature packs them closer toward the top and bottom, by up to
		// 1 + 4 * curvature at the corners.
		float scanline_count = 120.0f;

		// 0..1. Dark rows thin more than bright ones.
		float scanline_strength = 0.0f;

		// 0..1. An aperture grille three output pixels wide.
		float mask_strength = 0.0f;

		// 0..1. Darker toward the curved edge.
		float vignette = 0.0f;

		// Back-buffer pixels of red/blue split at the screen's edge.
		float aberration = 0.0f;

		// A gain restoring what the mask and the beam take away; 1 is none.
		float brightness = 1.0f;
	};

	// The fewest output pixels one scanline may cover. Fewer alias into
	// moire bands once curvature bends the rows.
	inline constexpr float min_pixels_per_scanline = 4.0f;

	// Packs settings into the CRT material's layout, dividing the screen by
	// the back-buffer size. Throws std::invalid_argument for a NaN or
	// infinite value, a negative value, a 0..1 strength above 1, a screen
	// with no area, a back buffer with no area, or, with scanlines on, a
	// count giving fewer than min_pixels_per_scanline output pixels a line at
	// the screen's centre.
	PostValues crt_values(const CrtSettings& settings,
		const Vector2F& back_buffer_size);

	// Whether values, in crt_values' layout, turn every CRT term off - or
	// have no screen, as an effect whose values were never set has none. A
	// backend draws such a CRT as identity, which is exact where resampling
	// through the CRT's arithmetic is not.
	bool crt_values_are_neutral(const PostValues& values);

	// Where on the flat picture the CRT samples for the point output_pixel,
	// in back-buffer pixels: the material's curvature, on the CPU. A point
	// outside the screen, or any point when the screen has no area, is its
	// own answer. Empty for a point inside the screen whose curved position
	// falls past the tube's edge, which the material draws black. A pointer
	// over the curved picture is mapped through this to find what it is over.
	std::optional<Vector2F> crt_source_position(const CrtSettings& settings,
		const Vector2F& output_pixel);
}

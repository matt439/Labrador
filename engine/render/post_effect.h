#pragma once

#include <array>

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
	};

	// The number of PostEffect values, for a table indexed by one.
	inline constexpr int post_effect_count = 1;

	// The frame's material_values (engine/render/post_frame.hlsli) as the
	// caller fills them: eight groups of four floats, read by a material as
	// material_value(0) to material_value(7).
	struct PostValues
	{
		std::array<float, 32> values{};
	};
}

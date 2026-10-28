#pragma once

#include "engine/core/name_table.h"
#include "engine/math/rectanglei.h"
#include "engine/render/animation_strip.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"

#include <memory>
#include <string>
#include <utility>

namespace components::animation_tests
{
	// Timing tests need strip data, not a window, a device or an atlas file.
	inline void add_sheet(labrador::RenderResources& resources,
		const std::string& name, bool looping = true, int count = 4,
		float frame_time = 0.25f, labrador::TextureHandle texture = {})
	{
		labrador::NameTable<labrador::SpriteFrame> frames("sprite frame");
		labrador::NameTable<labrador::AnimationStrip> strips("animation strip");
		strips.add("preview", labrador::AnimationStrip(
			labrador::RectangleI(0, 0, 16, 16), count, frame_time, looping));
		resources.add_sprite_sheet(name, std::make_unique<labrador::SpriteSheet>(
			texture, std::move(frames), std::move(strips)));
	}
}

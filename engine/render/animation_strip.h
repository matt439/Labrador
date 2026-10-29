#pragma once

#include "engine/math/rectanglei.h"

#include <vector>

namespace labrador
{
	class AnimationStrip
	{
	public:
		AnimationStrip() = default;

		// Caches horizontal frames spaced by first_frame.width. Zero frames
		// leave the cache empty without calculating an origin. Throws
		// std::out_of_range if a cached x coordinate cannot fit in int.
		AnimationStrip(const RectangleI& first_frame,
			int frame_count, float frame_time, bool looping);

		// A RectangleI rather than a RECT*: see SpriteFrame::source_rectangle.
		// By const reference rather than by value, into the strip's own cache.
		// Throws std::out_of_range for a frame index this strip does not have.
		const RectangleI& frame_rect(int frame_index) const;
		int frame_count() const;
		float frame_time() const;
		bool looping() const;

	private:
		std::vector<RectangleI> frame_rects_;
		RectangleI first_frame_ = RectangleI::zero;
		int frame_count_ = 0;
		float frame_time_ = 0.0f;
		bool looping_ = false;

		std::vector<RectangleI> calculate_all_frame_rects() const;
		RectangleI calculate_frame(int frame_index) const;
	};
}

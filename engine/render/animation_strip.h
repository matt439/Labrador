#pragma once

#include "engine/math/rectanglei.h"

#include <vector>

namespace labrador
{
	class AnimationStrip
	{
	public:
		AnimationStrip() = default;
		AnimationStrip(const mattmath::RectangleI& first_frame,
			int frame_count, float frame_time, bool looping);

		// By value, and a RectangleI rather than a RECT*: see
		// SpriteFrame::source_rectangle. Throws std::out_of_range for a frame
		// index this strip does not have.
		const mattmath::RectangleI& frame_rect(int frame_index) const;
		int frame_count() const;
		float frame_time() const;
		bool looping() const;

	private:
		std::vector<mattmath::RectangleI> frame_rects_;
		mattmath::RectangleI first_frame_ = mattmath::RectangleI::ZERO;
		int frame_count_ = 0;
		float frame_time_ = 0.0f;
		bool looping_ = false;

		std::vector<mattmath::RectangleI> calculate_all_frame_rects() const;
		mattmath::RectangleI calculate_frame(int frame_index) const;
	};
}

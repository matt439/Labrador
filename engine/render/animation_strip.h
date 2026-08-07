#pragma once

#include "engine/math/matt_math.h"

namespace artattack
{
	class AnimationStrip
	{
	public:
		AnimationStrip() = default;
		AnimationStrip(const mattmath::RectangleI& first_frame,
			int frame_count, float frame_time, bool looping);

		const RECT* frame_rect(int frame_index) const;
		int frame_count() const;
		float frame_time() const;
		bool looping() const;

	private:
		std::vector<RECT> frame_rects_;
		mattmath::RectangleI first_frame_ = { 0, 0, 0, 0 };
		int frame_count_ = 0;
		float frame_time_ = 0.0f;
		bool looping_ = false;

		std::vector<RECT> calculate_all_frame_rects() const;
		RECT calculate_frame(int frame_index) const;
	};
}

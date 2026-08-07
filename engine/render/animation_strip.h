#pragma once

#include "engine/math/matt_math.h"

class AnimationStrip
{
public:
	AnimationStrip() = default;
	AnimationStrip(const mattmath::RectangleI& first_frame,
		int frame_count, float frame_time, bool looping);

	const RECT* get_frame_rect(int frame_index) const;
	int get_frame_count() const;
	float get_frame_time() const;
	bool get_looping() const;

private:
	std::vector<RECT> frame_rects_;
	mattmath::RectangleI first_frame_ = { 0, 0, 0, 0 };
	int frame_count_ = 0;
	float frame_time_ = 0.0f;
	bool looping_ = false;

	std::vector<RECT> calculate_all_frame_rects() const;
	RECT calculate_frame(int frame_index) const;
};

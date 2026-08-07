#pragma once

#include "engine/math/matt_math.h"

class AnimationStrip
{
public:
	AnimationStrip() = default;
	AnimationStrip(const MattMath::RectangleI& first_frame,
		int frame_count, float frame_time, bool looping);

	const RECT* get_frame_rect(int frame_index) const;
	int get_frame_count() const;
	float get_frame_time() const;
	bool get_looping() const;

private:
	std::vector<RECT> _frame_rects;
	MattMath::RectangleI _first_frame = { 0, 0, 0, 0 };
	int _frame_count = 0;
	float _frame_time = 0.0f;
	bool _looping = false;

	std::vector<RECT> calculate_all_frame_rects() const;
	RECT calculate_frame(int frame_index) const;
};

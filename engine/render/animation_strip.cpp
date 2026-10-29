#include "engine/render/animation_strip.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace labrador;

namespace labrador
{
	AnimationStrip::AnimationStrip(const RectangleI& first_frame,
		int frame_count, float frame_time, bool looping) :
		first_frame_(first_frame),
		frame_count_(frame_count),
		frame_time_(frame_time),
		looping_(looping)
	{
		this->frame_rects_ = this->calculate_all_frame_rects();
	}

	RectangleI AnimationStrip::calculate_frame(int frame_index) const
	{
		if (frame_index < 0 || frame_index >= this->frame_count_)
		{
			throw std::out_of_range("Animation frame index " +
				std::to_string(frame_index) + " is outside a strip of " +
				std::to_string(this->frame_count_) + ".");
		}
		// The product can exceed int even when the origin cancels it back
		// into range. Check the complete coordinate before narrowing it.
		const int64_t frame_x = static_cast<int64_t>(this->first_frame_.x) +
			static_cast<int64_t>(frame_index) *
			static_cast<int64_t>(this->first_frame_.width);
		if (frame_x < (std::numeric_limits<int>::min)() ||
			frame_x > (std::numeric_limits<int>::max)())
		{
			throw std::out_of_range("Animation frame " +
				std::to_string(frame_index) + " x coordinate " +
				std::to_string(frame_x) + " is outside int range.");
		}
		RectangleI frame = this->first_frame_;
		frame.x = static_cast<int>(frame_x);
		return frame;
	}

	const RectangleI& AnimationStrip::frame_rect(int frame_index) const
	{
		if (frame_index < 0 || frame_index >= this->frame_count_)
		{
			throw std::out_of_range("Animation frame index " +
				std::to_string(frame_index) + " is outside a strip of " +
				std::to_string(this->frame_count_) + ".");
		}
		return this->frame_rects_[static_cast<size_t>(frame_index)];
	}

	int AnimationStrip::frame_count() const
	{
		return this->frame_count_;
	}
	float AnimationStrip::frame_time() const
	{
		return this->frame_time_;
	}
	bool AnimationStrip::looping() const
	{
		return this->looping_;
	}

	std::vector<RectangleI> AnimationStrip::calculate_all_frame_rects() const
	{
		// Origins form a linear sequence, so its endpoints bound the cache.
		// Validate before reserving; an empty strip computes no coordinate.
		if (this->frame_count_ > 0)
		{
			static_cast<void>(this->calculate_frame(this->frame_count_ - 1));
		}
		std::vector<RectangleI> rects;
		rects.reserve(static_cast<size_t>(this->frame_count_));
		for (int i = 0; i < this->frame_count_; i++)
		{
			rects.push_back(this->calculate_frame(i));
		}
		return rects;
	}
}

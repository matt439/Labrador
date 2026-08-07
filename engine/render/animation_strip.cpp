#include "engine/render/animation_strip.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
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

	RECT AnimationStrip::calculate_frame(int frame_index) const
	{
		if (frame_index < 0 || frame_index >= this->frame_count_)
		{
			throw std::exception("frame_index out of range");
		}
		RectangleI frame = this->first_frame_;
		frame.offset(frame_index * frame.width, 0);
		return frame.win_rect();
	}

	const RECT* AnimationStrip::frame_rect(int frame_index) const
	{
		if (frame_index < 0 || frame_index >= this->frame_count_)
		{
			throw std::exception("frame_index out of range");
		}
		return &this->frame_rects_[frame_index];
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

	std::vector<RECT> AnimationStrip::calculate_all_frame_rects() const
	{
		std::vector<RECT> rects;
		for (int i = 0; i < this->frame_count_; i++)
		{
			rects.push_back(this->calculate_frame(i));
		}
		return rects;
	}
}
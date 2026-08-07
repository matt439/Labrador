#include "engine/render/animation_strip.h"

using namespace DirectX;
using namespace MattMath;

AnimationStrip::AnimationStrip(const RectangleI& first_frame,
	int frame_count, float frame_time, bool looping) :
	_first_frame(first_frame),
	_frame_count(frame_count),
	_frame_time(frame_time),
	_looping(looping)
{
	this->_frame_rects = this->calculate_all_frame_rects();
}

RECT AnimationStrip::calculate_frame(int frame_index) const
{
	if (frame_index < 0 || frame_index >= this->_frame_count)
	{
		throw std::exception("frame_index out of range");
	}
	RectangleI frame = this->_first_frame;
	frame.offset(frame_index * frame.width, 0);
	return frame.get_win_rect();
}

const RECT* AnimationStrip::get_frame_rect(int frame_index) const
{
	if (frame_index < 0 || frame_index >= this->_frame_count)
	{
		throw std::exception("frame_index out of range");
	}
	return &this->_frame_rects[frame_index];
}

int AnimationStrip::get_frame_count() const
{
	return this->_frame_count;
}
float AnimationStrip::get_frame_time() const
{
	return this->_frame_time;
}
bool AnimationStrip::get_looping() const
{
	return this->_looping;
}

std::vector<RECT> AnimationStrip::calculate_all_frame_rects() const
{
	std::vector<RECT> rects;
	for (int i = 0; i < this->_frame_count; i++)
	{
		rects.push_back(this->calculate_frame(i));
	}
	return rects;
}
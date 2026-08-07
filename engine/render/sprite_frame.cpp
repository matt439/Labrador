#include "engine/render/sprite_frame.h"

using namespace mattmath;

namespace artattack
{
	SpriteFrame::SpriteFrame(const RectangleI& source_rectangle,
		const Vector2F& origin,
		bool rotated)
	{
		this->set_source_rectangle(source_rectangle);
		this->origin_ = origin;
		this->rotated_ = rotated;
	}
	SpriteFrame::SpriteFrame(const Vector2F& position, const Vector2F& size,
		const Vector2F& origin, bool rotated)
	{
		this->set_source_rectangle(position, size);
		this->origin_ = origin;
		this->rotated_ = rotated;
	}

	const RECT* SpriteFrame::source_rectangle() const
	{
		return &this->source_rectangle_;
	}


	void SpriteFrame::set_source_rectangle(const RectangleI& source_rectangle)
	{
		this->source_rectangle_ = source_rectangle.win_rect();
	}

	void SpriteFrame::set_source_rectangle(const Vector2F& position,
		const Vector2F& size)
	{
		auto source_rectan = RectangleI(position, size);
		this->source_rectangle_ = source_rectan.win_rect();
	}
}
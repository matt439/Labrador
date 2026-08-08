#include "engine/render/sprite_frame.h"

using namespace mattmath;

namespace artattack
{
	SpriteFrame::SpriteFrame(const RectangleI& source_rectangle,
		const Vector2F& origin,
		bool rotated) :
		source_rectangle_(source_rectangle),
		origin_(origin),
		rotated_(rotated)
	{

	}
	SpriteFrame::SpriteFrame(const Vector2F& position, const Vector2F& size,
		const Vector2F& origin, bool rotated) :
		source_rectangle_(position, size),
		origin_(origin),
		rotated_(rotated)
	{

	}

	const RectangleI& SpriteFrame::source_rectangle() const
	{
		return this->source_rectangle_;
	}
}

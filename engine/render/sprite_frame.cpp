#include "engine/render/sprite_frame.h"

using namespace mattmath;

namespace labrador
{
	SpriteFrame::SpriteFrame(const RectangleI& source_rectangle,
		const Vector2F& origin) :
		source_rectangle_(source_rectangle),
		origin_(origin)
	{

	}
	SpriteFrame::SpriteFrame(const Vector2F& position, const Vector2F& size,
		const Vector2F& origin) :
		source_rectangle_(position, size),
		origin_(origin)
	{

	}

	const RectangleI& SpriteFrame::source_rectangle() const
	{
		return this->source_rectangle_;
	}

	const Vector2F& SpriteFrame::origin() const
	{
		return this->origin_;
	}
}

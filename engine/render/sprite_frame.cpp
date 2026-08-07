#include "engine/render/sprite_frame.h"

using namespace MattMath;

SpriteFrame::SpriteFrame(const RectangleI& source_rectangle,
	const Vector2F& origin,
	bool rotated)
{
	this->set_source_rectangle(source_rectangle);
	this->_origin = origin;
	this->_rotated = rotated;
}
SpriteFrame::SpriteFrame(const Vector2F& position, const Vector2F& size,
	const Vector2F& origin, bool rotated)
{
	this->set_source_rectangle(position, size);
	this->_origin = origin;
	this->_rotated = rotated;
}

const RECT* SpriteFrame::get_source_rectangle() const
{
	return &this->_source_rectangle;
}


void SpriteFrame::set_source_rectangle(const RectangleI& source_rectangle)
{
	this->_source_rectangle = source_rectangle.get_win_rect();
}

void SpriteFrame::set_source_rectangle(const Vector2F& position,
	const Vector2F& size)
{
	auto source_rectan = RectangleI(position, size);
	this->_source_rectangle = source_rectan.get_win_rect();
}
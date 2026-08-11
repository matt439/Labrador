#include "engine/math/rectanglei.h"

#include "engine/math/vector2f.h"
#include "engine/math/rectanglef.h"

namespace mattmath
{

	RectangleI::RectangleI(int x, int y, int width, int height)
	{
		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;
	}
	RectangleI::RectangleI(const Vector2I& position,
		const Vector2I& size)
	{
		this->x = position.x;
		this->y = position.y;
		this->width = size.x;
		this->height = size.y;
	}
	RectangleI::RectangleI(const Vector2F& position,const Vector2F& size)
	{
		this->x = static_cast<int>(position.x);
		this->y = static_cast<int>(position.y);
		this->width = static_cast<int>(size.x);
		this->height = static_cast<int>(size.y);
	}
	RectangleI::RectangleI(const RectangleF& rectangle)
	{
		this->x = static_cast<int>(rectangle.x);
		this->y = static_cast<int>(rectangle.y);
		this->width = static_cast<int>(rectangle.width);
		this->height = static_cast<int>(rectangle.height);
	}
	int RectangleI::left() const
	{
		return this->x;
	}
	int RectangleI::top() const
	{
		return this->y;
	}
	int RectangleI::right() const
	{
		return this->x + this->width;
	}
	int RectangleI::bottom() const
	{
		return this->y + this->height;
	}
	Vector2I RectangleI::position() const
	{
		return Vector2I(this->x, this->y);
	}
	Vector2I RectangleI::size() const
	{
		return Vector2I(this->width, this->height);
	}
	Vector2I RectangleI::top_left() const
	{
		return Vector2I(this->x, this->y);
	}
	Vector2I RectangleI::bottom_right() const
	{
		return Vector2I(this->x + this->width, this->y + this->height);
	}
	bool RectangleI::operator==(const RectangleI& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->width == other.width &&
			this->height == other.height;
	}
	bool RectangleI::operator!=(const RectangleI& other) const
	{
		return !(*this == other);
	}
	bool RectangleI::contains(const Vector2I& point) const
	{
		return point.x >= this->x &&
			point.x <= this->x + this->width &&
			point.y >= this->y &&
			point.y <= this->y + this->height;

	}
	bool RectangleI::contains(const RectangleI& other) const
	{
		return other.x >= this->x &&
			other.x + other.width <= this->x + this->width &&
			other.y >= this->y &&
			other.y + other.height <= this->y + this->height;

	}
	void RectangleI::offset(int horizontal_amount, int vertical_amount)
	{
		this->x += horizontal_amount;
		this->y += vertical_amount;
	}
	void RectangleI::offset(const Vector2I& amount)
	{
		this->x += amount.x;
		this->y += amount.y;
	}
	void RectangleI::set_left(int left)
	{
		this->width += this->x - left;
		this->x = left;
	}
	void RectangleI::set_top(int top)
	{
		this->height += this->y - top;
		this->y = top;
	}
	void RectangleI::set_right(int right)
	{
		this->width = right - this->x;
	}
	void RectangleI::set_bottom(int bottom)
	{
		this->height = bottom - this->y;
	}
	void RectangleI::set_position(const Vector2I& position)
	{
		this->x = position.x;
		this->y = position.y;
	}
	void RectangleI::set_size(const Vector2I& size)
	{
		this->width = size.x;
		this->height = size.y;
	}
	void RectangleI::set_top_left(const Vector2I& top_left)
	{
		this->width += this->x - top_left.x;
		this->height += this->y - top_left.y;
		this->x = top_left.x;
		this->y = top_left.y;
	}
	void RectangleI::set_bottom_right(const Vector2I& bottom_right)
	{
		this->width = bottom_right.x - this->x;
		this->height = bottom_right.y - this->y;
	}
	void RectangleI::set_top_left_and_bottom_right(const Vector2I& top_left,
		const Vector2I& bottom_right)
	{
		this->width = bottom_right.x - top_left.x;
		this->height = bottom_right.y - top_left.y;
		this->x = top_left.x;
		this->y = top_left.y;
	}
	const RectangleI RectangleI::ZERO = { 0, 0, 0, 0 };

}

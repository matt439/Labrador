#include "engine/math/rectanglef.h"

#include "engine/math/rectanglei.h"
#include "engine/math/ericson_math.h"

#include <algorithm>
#include <span>
#include <cmath>

namespace mattmath
{

	RectangleF::RectangleF(float x, float y, float width, float height) :
		x(x), y(y), width(width), height(height)
	{
	}
	RectangleF::RectangleF(const Vector2F& position,
		const Vector2F& size)
	{
		this->x = position.x;
		this->y = position.y;
		this->width = size.x;
		this->height = size.y;
	}
	RectangleF::RectangleF(const Vector2F& center, float horiz_half_width,
		float vert_half_height)
	{
		this->x = center.x - horiz_half_width;
		this->y = center.y - vert_half_height;
		this->width = horiz_half_width * 2.0f;
		this->height = vert_half_height * 2.0f;
	}
	RectangleF RectangleF::bounding_box() const
	{
		return *this;
	}
	ShapeType RectangleF::shape_type() const
	{
		return ShapeType::rectangle;
	}
	Vector2F RectangleF::center() const
	{
		return Vector2F(this->x + this->width / 2.0f,
					this->y + this->height / 2.0f);
	}
	Vector2F RectangleF::position() const
	{
		return Vector2F(this->x, this->y);
	}
	Vector2F RectangleF::size() const
	{
		return Vector2F(this->width, this->height);
	}
	float RectangleF::left() const
	{
		return this->x;
	}
	float RectangleF::right() const
	{
		return this->x + this->width;
	}
	float RectangleF::top() const
	{
		return this->y;
	}
	float RectangleF::bottom() const
	{
		return this->y + this->height;
	}
	Vector2F RectangleF::top_left() const
	{
		return Vector2F(this->x, this->y);
	}
	Vector2F RectangleF::bottom_right() const
	{
		return Vector2F(this->x + this->width, this->y + this->height);
	}
	Vector2F RectangleF::top_right() const
	{
		return Vector2F(this->x + this->width, this->y);
	}
	Vector2F RectangleF::bottom_left() const
	{
		return Vector2F(this->x, this->y + this->height);
	}
	Segment RectangleF::top_edge() const
	{
		return Segment(this->top_left(), this->top_right());
	}
	Segment RectangleF::bottom_edge() const
	{
		return Segment(this->bottom_left(), this->bottom_right());
	}
	Segment RectangleF::left_edge() const
	{
		return Segment(this->top_left(), this->bottom_left());
	}
	Segment RectangleF::right_edge() const
	{
		return Segment(this->top_right(), this->bottom_right());
	}
	std::array<Segment, 4> RectangleF::edges() const
	{
		return
		{
			this->top_edge(),
			this->bottom_edge(),
			this->left_edge(),
			this->right_edge()
		};
	}
	RectangleI RectangleF::rectangle_i() const
	{
		return RectangleI(static_cast<int>(this->x),
			static_cast<int>(this->y),
			static_cast<int>(this->width),
			static_cast<int>(this->height));
	}
	bool RectangleF::operator==(const RectangleF& other) const
	{
		return this->x == other.x &&
			this->y == other.y &&
			this->width == other.width &&
			this->height == other.height;
	}
	bool RectangleF::operator!=(const RectangleF& other) const
	{
		return !(*this == other);
	}
	//bool RectangleF::contains(const Vector2F& point) const
	//{
	//	return point.x >= this->x &&
	//		point.x <= this->x + this->width &&
	//		point.y >= this->y &&
	//		point.y <= this->y + this->height;
	//}
	bool RectangleF::contains(const RectangleF& other) const
	{
		return other.x >= this->x &&
			other.x + other.width <= this->x + this->width &&
			other.y >= this->y &&
			other.y + other.height <= this->y + this->height;
	}
	bool RectangleF::intersects(const RectangleF& other) const
	{
		return test_AABB_AABB(*this, other);
	}
	void RectangleF::inflate(float horizontal_amount, float vertical_amount)
	{
		this->x -= horizontal_amount;
		this->y -= vertical_amount;
		this->width += horizontal_amount * 2.0f;
		this->height += vertical_amount * 2.0f;
	}
	void RectangleF::inflate(const Vector2F& amount)
	{
		this->x -= amount.x;
		this->y -= amount.y;
		this->width += amount.x * 2.0f;
		this->height += amount.y * 2.0f;
	}
	void RectangleF::inflate(float amount)
	{
		this->inflate(amount, amount);
	}
	void RectangleF::offset(float horizontal_amount, float vertical_amount)
	{
		this->x += horizontal_amount;
		this->y += vertical_amount;
	}
	void RectangleF::offset(const mattmath::Vector2F& amount)
	{
		this->x += amount.x;
		this->y += amount.y;
	}
	void RectangleF::scale(float horizontal_amount, float vertical_amount)
	{
		this->width *= horizontal_amount;
		this->height *= vertical_amount;
	}
	void RectangleF::scale(const Vector2F& amount)
	{
		this->width *= amount.x;
		this->height *= amount.y;
	}
	void RectangleF::set_position(const Vector2F& position)
	{
		this->x = position.x;
		this->y = position.y;
	}
	void RectangleF::set_position(float new_x, float new_y)
	{
		this->x = new_x;
		this->y = new_y;
	}
	void RectangleF::set_position_at_center(const Vector2F& position)
	{
		this->x = position.x - this->width / 2.0f;
		this->y = position.y - this->height / 2.0f;
	}
	void RectangleF::set_position_at_center(float new_x, float new_y)
	{
		this->x = new_x - this->width / 2.0f;
		this->y = new_y - this->height / 2.0f;
	}
	void RectangleF::set_position_x(float new_x)
	{
		this->x = new_x;
	}
	void RectangleF::set_position_x_from_right(float new_x)
	{
		this->x = new_x - this->width;
	}
	void RectangleF::set_position_y(float new_y)
	{
		this->y = new_y;
	}
	void RectangleF::set_position_y_from_bottom(float new_y)
	{
		this->y = new_y - this->height;
	}
	void RectangleF::set_position_from_top_right(const Vector2F& position)
	{
		this->x = position.x - this->width;
		this->y = position.y;
	}
	void RectangleF::set_position_from_top_right(float new_x, float new_y)
	{
		this->x = new_x - this->width;
		this->y = new_y;
	}
	void RectangleF::set_position_from_bottom_left(const Vector2F& position)
	{
		this->x = position.x;
		this->y = position.y - this->height;
	}
	void RectangleF::set_position_from_bottom_left(float new_x, float new_y)
	{
		this->x = new_x;
		this->y = new_y - this->height;
	}
	void RectangleF::set_position_from_bottom_right(const Vector2F& position)
	{
		this->x = position.x - this->width;
		this->y = position.y - this->height;
	}
	void RectangleF::set_position_from_bottom_right(float new_x, float new_y)
	{
		this->x = new_x - this->width;
		this->y = new_y - this->height;
	}
	void RectangleF::set_size(const Vector2F& size)
	{
		this->width = size.x;
		this->height = size.y;
	}
	void RectangleF::set_size(float new_width, float new_height)
	{
		this->width = new_width;
		this->height = new_height;
	}
	void RectangleF::set_width(float new_width)
	{
		this->width = new_width;
	}
	void RectangleF::set_height(float new_height)
	{
		this->height = new_height;
	}
	RectangleF RectangleF::bounding_box_of(std::span<const Point2F> points)
	{
		if (points.empty())
		{
			return RectangleF::ZERO;
		}

		float min_x = points[0].x;
		float max_x = points[0].x;
		float min_y = points[0].y;
		float max_y = points[0].y;

		for (size_t i = 1; i < points.size(); i++)
		{
			min_x = std::min(min_x, points[i].x);
			max_x = std::max(max_x, points[i].x);
			min_y = std::min(min_y, points[i].y);
			max_y = std::max(max_y, points[i].y);
		}

		return RectangleF(min_x, min_y, max_x - min_x, max_y - min_y);
	}
	RectangleF RectangleF::union_of(const RectangleF& a, const RectangleF& b)
	{
		float x1 = std::min(a.x, b.x);
		float x2 = std::max(a.x + a.width, b.x + b.width);
		float y1 = std::min(a.y, b.y);
		float y2 = std::max(a.y + a.height, b.y + b.height);
		return RectangleF(x1, y1, x2 - x1, y2 - y1);
	}
	RectangleF RectangleF::from_top_left_bottom_right(const Vector2F& top_left,
		const Vector2F& bottom_right)
	{
		return RectangleF(top_left.x, top_left.y,
			bottom_right.x - top_left.x,
			bottom_right.y - top_left.y);
	}
	RectangleF RectangleF::from_top_left_bottom_right(float top, float left,
		float bottom, float right)
	{
		return RectangleF(left, top, right - left, bottom - top);
	}
	const RectangleF RectangleF::ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };

}

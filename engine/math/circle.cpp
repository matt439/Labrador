#include "engine/math/circle.h"

#include "engine/math/rectanglef.h"

namespace mattmath
{

	Circle::Circle(const Vector2F& center, float radius) :
		center_(center), radius_(radius)
	{
	}
	Circle::Circle(float x, float y, float radius)
	{
		this->center_ = Vector2F(x, y);
		this->radius_ = radius;
	}
	RectangleF Circle::bounding_box() const
	{
		return mattmath::RectangleF(this->center_.x - this->radius_,
					this->center_.y - this->radius_,
					this->radius_ * 2.0f, this->radius_ * 2.0f);

	}
	ShapeType Circle::shape_type() const
	{
		return ShapeType::circle;
	}

	void Circle::offset(const Vector2F& offset)
	{
		this->center_ += offset;
	}

	void Circle::inflate(float amount)
	{
		this->radius_ += amount;
	}

	bool Circle::operator==(const Circle& other) const
	{
		return this->center_ == other.center_ &&
			this->radius_ == other.radius_;
	}
	bool Circle::operator!=(const Circle& other) const
	{
		return !(*this == other);
	}
	//bool Circle::contains(const Vector2F& point) const
	//{
	//	return Vector2F::distance(this->center_, point) <= this->radius_;
	//}
	Point2F Circle::center() const
	{
		return this->center_;
	}
	float Circle::radius() const
	{
		return this->radius_;
	}

}

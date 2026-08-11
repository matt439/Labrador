#include "engine/math/segment.h"

#include <cmath>

namespace mattmath
{

	Segment::Segment(const Point2F& point_0, const Point2F& point_1) :
		point_0(point_0), point_1(point_1)
	{
	}
	Segment::Segment(float x0, float y0, float x1, float y1) :
		point_0(x0, y0), point_1(x1, y1)
	{
	}
	bool Segment::operator==(const Segment& other) const
	{
		return this->point_0 == other.point_0 &&
			this->point_1 == other.point_1;
	}
	bool Segment::operator!=(const Segment& other) const
	{
		return !(*this == other);
	}
	Vector2F Segment::direction() const
	{
		return this->point_1 - this->point_0;
	}
	float Segment::length() const
	{
		return this->direction().length();
	}
	Point2F Segment::center() const
	{
		return (this->point_0 + this->point_1) / 2.0f;
	}

}

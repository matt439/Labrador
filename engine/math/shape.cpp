#include "engine/math/shape.h"

#include "engine/math/rectanglef.h"

namespace mattmath
{

	bool Shape::AABB_intersects(const Shape* other) const
	{
		return this->bounding_box().intersects(other->bounding_box());
	}

	bool Shape::AABB_intersects(const Shape& other) const
	{
		return this->AABB_intersects(&other);
	}

}

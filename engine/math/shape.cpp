#include "engine/math/shape.h"

#include "engine/math/rectanglef.h"

namespace labrador
{

	bool Shape::bounding_boxes_intersect(const Shape& other) const
	{
		return this->bounding_box().intersects(other.bounding_box());
	}

}

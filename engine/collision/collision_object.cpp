#include "engine/collision/collision_object.h"

#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace labrador
{
	namespace
	{
		[[noreturn]] void refuse(const CollisionObject& object, const char* field)
		{
			throw std::invalid_argument("CollisionObject::validate_finite_shape: tag " +
				std::to_string(object.tag()) + " " + field);
		}

		void finite(const CollisionObject& object, float value, const char* field)
		{
			if (!std::isfinite(value))
			{
				refuse(object, field);
			}
		}

		void finite(const CollisionObject& object, const Vector2F& point,
			const char* field)
		{
			if (!std::isfinite(point.x) || !std::isfinite(point.y))
			{
				refuse(object, field);
			}
		}
	}

	void CollisionObject::validate_finite_shape() const
	{
		const Shape* const shape = this->shape();
		if (shape == nullptr)
		{
			refuse(*this, "shape() is null");
		}

		switch (shape->shape_type())
		{
		case ShapeType::rectangle:
		{
			// The rectangle discriminator permits a Shape with an AABB, just as
			// narrow_phase's rectangle path does; no concrete downcast is needed.
			const RectangleF box = shape->bounding_box();
			finite(*this, box.x, "rectangle.x must be finite");
			finite(*this, box.y, "rectangle.y must be finite");
			finite(*this, box.width, "rectangle.width must be finite");
			finite(*this, box.height, "rectangle.height must be finite");
			return;
		}
		case ShapeType::circle:
		{
			const Circle& circle = static_cast<const Circle&>(*shape);
			finite(*this, circle.center(), "circle.center must be finite");
			finite(*this, circle.radius(), "circle.radius must be finite");
			return;
		}
		case ShapeType::triangle:
		{
			const Triangle& triangle = static_cast<const Triangle&>(*shape);
			finite(*this, triangle.point_0(), "triangle.point_0 must be finite");
			finite(*this, triangle.point_1(), "triangle.point_1 must be finite");
			finite(*this, triangle.point_2(), "triangle.point_2 must be finite");
			return;
		}
		case ShapeType::quad:
		{
			const Quad& quad = static_cast<const Quad&>(*shape);
			finite(*this, quad.point_0(), "quad.point_0 must be finite");
			finite(*this, quad.point_1(), "quad.point_1 must be finite");
			finite(*this, quad.point_2(), "quad.point_2 must be finite");
			finite(*this, quad.point_3(), "quad.point_3 must be finite");
			return;
		}
		case ShapeType::rectangle_rotated:
		{
			const RectangleRotated& rectangle =
				static_cast<const RectangleRotated&>(*shape);
			finite(*this, rectangle.center(), "rectangle_rotated.center must be finite");
			finite(*this, rectangle.x_axis(), "rectangle_rotated.x_axis must be finite");
			finite(*this, rectangle.y_axis(), "rectangle_rotated.y_axis must be finite");
			finite(*this, rectangle.half_extents(),
				"rectangle_rotated.half_extents must be finite");
			return;
		}
		case ShapeType::none:
		default:
			refuse(*this, "shape has no supported type");
		}
	}
}

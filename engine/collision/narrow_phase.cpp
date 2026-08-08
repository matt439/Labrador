#include "engine/collision/narrow_phase.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

using mattmath::RectangleF;
using mattmath::Shape;
using mattmath::ShapeType;
using mattmath::Vector2F;

namespace artattack
{
	namespace
	{
		// Every collidable shape in the engine is a convex polygon of at most
		// four vertices, so the narrow phase carries them in an array rather
		// than asking Shape::edges(), which returns a std::vector - a heap
		// allocation per shape per pair per frame, on the one path
		// PHILOSOPHY's performance section names outright.
		struct Polygon
		{
			static constexpr int MAX_POINTS = 4;

			Vector2F points[MAX_POINTS];
			int count = 0;
		};

		// The downcasts are safe because shape_type() is the discriminator
		// every Shape implements, and they are static because this runs per
		// pair per frame.
		Polygon polygon_from(const Shape& shape)
		{
			Polygon polygon;

			switch (shape.shape_type())
			{
			case ShapeType::rectangle:
			{
				// Axis-aligned, so its own bounding box is the shape - no
				// cast needed and no precision lost.
				const RectangleF box = shape.bounding_box();
				polygon.points[0] = box.top_left();
				polygon.points[1] = box.top_right();
				polygon.points[2] = box.bottom_right();
				polygon.points[3] = box.bottom_left();
				polygon.count = 4;
				return polygon;
			}
			case ShapeType::triangle:
			{
				const auto& triangle = static_cast<const mattmath::Triangle&>(shape);
				polygon.points[0] = triangle.point_0();
				polygon.points[1] = triangle.point_1();
				polygon.points[2] = triangle.point_2();
				polygon.count = 3;
				return polygon;
			}
			case ShapeType::quad:
			{
				const auto& quad = static_cast<const mattmath::Quad&>(shape);
				polygon.points[0] = quad.point_0();
				polygon.points[1] = quad.point_1();
				polygon.points[2] = quad.point_2();
				polygon.points[3] = quad.point_3();
				polygon.count = 4;
				return polygon;
			}
			case ShapeType::rectangle_rotated:
			{
				const mattmath::Quad quad(
					static_cast<const mattmath::RectangleRotated&>(shape));
				polygon.points[0] = quad.point_0();
				polygon.points[1] = quad.point_1();
				polygon.points[2] = quad.point_2();
				polygon.points[3] = quad.point_3();
				polygon.count = 4;
				return polygon;
			}
			case ShapeType::circle:
				throw std::invalid_argument(
					"narrow_phase: a circle is not a convex polygon, and the "
					"circle axis is not implemented");
			case ShapeType::none:
			default:
				throw std::invalid_argument(
					"narrow_phase: shape has no type");
			}
		}

		void project(const Polygon& polygon, const Vector2F& axis,
			float& min, float& max)
		{
			min = Vector2F::dot(polygon.points[0], axis);
			max = min;
			for (int i = 1; i < polygon.count; i++)
			{
				const float distance = Vector2F::dot(polygon.points[i], axis);
				min = std::min(min, distance);
				max = std::max(max, distance);
			}
		}

		// What one candidate axis has to say about the pair.
		struct AxisTest
		{
			bool separated = true;
			// Signed so the caller does not have to guess which way to point
			// the normal: the axis, flipped if `a` is the one further along it.
			Vector2F normal = Vector2F::ZERO;
			float penetration = 0.0f;
		};

		// `axis` must be unit length, or the depths measured on different axes
		// are in different units and the smallest of them means nothing.
		AxisTest test_axis(const Polygon& a, const Polygon& b, const Vector2F& axis)
		{
			float a_min = 0.0f;
			float a_max = 0.0f;
			float b_min = 0.0f;
			float b_max = 0.0f;
			project(a, axis, a_min, a_max);
			project(b, axis, b_min, b_max);

			// The two ways off this axis, measured as the distance `a` would
			// travel: forwards until its trailing edge clears b, or backwards
			// until its leading edge does.
			//
			// Taking the smaller of these rather than the width of the
			// intersection is what makes containment work. When one interval
			// sits wholly inside the other the intersection is the inner
			// interval's whole width, and translating by that much leaves it
			// exactly as overlapped as it started - the two ways out are
			// longer than the overlap is wide, not shorter. That case is not a
			// curiosity: it is a player standing on a platform thinner than
			// they are tall, which is most of a platformer.
			const float forwards = b_max - a_min;
			const float backwards = a_max - b_min;

			if (forwards <= 0.0f || backwards <= 0.0f)
			{
				return AxisTest{};
			}

			AxisTest result;
			result.separated = false;
			if (backwards < forwards)
			{
				// b lies further along the axis, so the normal - which points
				// from a towards b - is the axis as given.
				result.normal = axis;
				result.penetration = backwards;
			}
			else
			{
				result.normal = Vector2F(-axis.x, -axis.y);
				result.penetration = forwards;
			}
			return result;
		}
	}

	std::optional<Manifold> narrow_phase(const Shape& a, const Shape& b)
	{
		const Polygon polygon_a = polygon_from(a);
		const Polygon polygon_b = polygon_from(b);

		if (polygon_a.count < 3 || polygon_b.count < 3)
		{
			return std::nullopt;
		}

		Manifold least;
		least.penetration = std::numeric_limits<float>::max();

		const Polygon* const polygons[2] = { &polygon_a, &polygon_b };
		for (const Polygon* polygon : polygons)
		{
			for (int i = 0; i < polygon->count; i++)
			{
				const Vector2F edge =
					polygon->points[(i + 1) % polygon->count] - polygon->points[i];

				// The edge's normal, normalised. normalized() reports a
				// zero-length vector as zero rather than inventing (1, 0) for
				// it, which is how a degenerate edge is skipped instead of
				// contributing a bogus axis.
				const Vector2F axis = Vector2F(-edge.y, edge.x).normalized();
				if (axis == Vector2F::ZERO)
				{
					continue;
				}

				const AxisTest test = test_axis(polygon_a, polygon_b, axis);
				if (test.separated)
				{
					// One separating axis is a proof of no contact, so this is
					// also the early out that makes the common case - two
					// things nowhere near each other - cost one projection
					// pair rather than all seven.
					return std::nullopt;
				}

				if (test.penetration < least.penetration)
				{
					least.normal = test.normal;
					least.penetration = test.penetration;
				}
			}
		}

		if (least.normal == Vector2F::ZERO)
		{
			// Every edge was degenerate, so neither shape has an interior.
			return std::nullopt;
		}

		return least;
	}
}

#include "engine/collision/narrow_phase.h"

#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/shape_type.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace labrador
{
	namespace
	{
		// Every collidable shape other than a circle is a convex polygon of at
		// most four vertices, so the narrow phase carries them in an array rather
		// than asking Shape::edges(), which returns a std::vector - a heap
		// allocation per shape per pair per frame, on the one path
		// PHILOSOPHY's performance section names outright.
		struct Polygon
		{
			static constexpr int max_points = 4;

			Vector2F points[max_points];
			int count = 0;

			// The candidate axes this shape contributes: its edge normals,
			// unit length, with duplicates already removed.
			//
			// Opposite edges of a rectangle are antiparallel, so their normals
			// differ only in sign - and testing an axis against its own
			// negative is provably inert. Projecting on -n negates and swaps
			// the interval, which swaps `forwards` and `backwards`, so the
			// smaller of the two is the same number and the normal comes back
			// correctly signed either way. The duplicate can therefore never
			// win the strict comparison in narrow_phase, and dropping it costs
			// nothing and saves half the work on the pair type that is
			// essentially all of this game's collision traffic.
			Vector2F axes[max_points];
			int axis_count = 0;
		};

		// Fills `axes` from the first `edge_count` edges. Degenerate edges
		// contribute nothing. Every represented nonzero edge matters, however
		// short: removing one can remove the only separating axis of a thin
		// polygon. Double intermediates keep subtraction and normalization from
		// overflowing or underflowing for finite float coordinates.
		void fill_axes(Polygon& polygon, int edge_count)
		{
			for (int i = 0; i < edge_count; i++)
			{
				const Vector2F& start = polygon.points[i];
				const Vector2F& end = polygon.points[(i + 1) % polygon.count];
				const double x = static_cast<double>(end.x) - start.x;
				const double y = static_cast<double>(end.y) - start.y;
				const double length = std::hypot(x, y);
				if (!(length > 0.0) || !std::isfinite(length))
				{
					continue;
				}

				polygon.axes[polygon.axis_count] =
					Vector2F(static_cast<float>(-y / length), static_cast<float>(x / length));
				polygon.axis_count++;
			}
		}

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

				// Known at compile time, and already unit. An axis-aligned
				// rectangle's edge normals are the cardinal directions
				// whatever its size or position, so this pair costs no
				// subtractions and, more to the point, no square roots.
				polygon.axes[0] = Vector2F(1.0f, 0.0f);
				polygon.axes[1] = Vector2F(0.0f, 1.0f);
				polygon.axis_count = 2;
				return polygon;
			}
			case ShapeType::triangle:
			{
				const Triangle& triangle = static_cast<const Triangle&>(shape);
				polygon.points[0] = triangle.point_0();
				polygon.points[1] = triangle.point_1();
				polygon.points[2] = triangle.point_2();
				polygon.count = 3;

				// No two edges of a triangle are parallel unless it is
				// degenerate, so all three normals are distinct.
				fill_axes(polygon, 3);
				return polygon;
			}
			case ShapeType::quad:
			{
				const Quad& quad = static_cast<const Quad&>(shape);
				polygon.points[0] = quad.point_0();
				polygon.points[1] = quad.point_1();
				polygon.points[2] = quad.point_2();
				polygon.points[3] = quad.point_3();
				polygon.count = 4;

				// A general convex quad has four distinct normals. It is
				// convex - Quad's constructor enforces that, and the
				// separating-axis theorem decides nothing otherwise.
				fill_axes(polygon, 4);
				return polygon;
			}
			case ShapeType::rectangle_rotated:
			{
				// Read straight off the shape, and NOT by building a
				// Quad from it: that constructor allocates and
				// validates twice over, inside the one function whose comment
				// above is about avoiding a heap allocation per shape per pair
				// per frame.
				const RectangleRotated& rotated =
					static_cast<const RectangleRotated&>(shape);
				polygon.points[0] = rotated.point_0();
				polygon.points[1] = rotated.point_1();
				polygon.points[2] = rotated.point_2();
				polygon.points[3] = rotated.point_3();
				polygon.count = 4;

				// The shape already validates these orthonormal axes. Reading
				// them avoids reconstructing a thin edge from rounded corners.
				polygon.axes[0] = rotated.x_axis();
				polygon.axes[1] = rotated.y_axis();
				polygon.axis_count = 2;
				return polygon;
			}
			case ShapeType::circle:
				// narrow_phase sends circles to their own measurements before
				// asking for a polygon, so only a new caller can reach this.
				throw std::invalid_argument(
					"narrow_phase: a circle is not a convex polygon");
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

		// Whether the polygon encloses anything at all.
		//
		// Degenerate edges are already skipped when the axes are built, but
		// that is not enough on its own: three collinear points produce three
		// perfectly good non-zero edge normals, all parallel. Projected onto
		// any of them the shape is a single point, so both ways off the axis
		// are positive whenever the other shape straddles that line, and the
		// pair reports a confident overlap against a triangle with no
		// interior.
		//
		// Zero area is exactly the condition, and it is checked exactly:
		// a very thin triangle is a real shape and the separating-axis test
		// handles it correctly. Triangle permits a collinear one -
		// its constructor validates nothing - so this is caught here rather
		// than being assumed away.
		bool has_interior(const Polygon& polygon)
		{
			for (int i = 0; i < polygon.count; i++)
			{
				if (!std::isfinite(polygon.points[i].x)
					|| !std::isfinite(polygon.points[i].y))
				{
					return false;
				}
			}

			// A fan around the first vertex avoids subtracting large products
			// of world coordinates. Double also retains tiny nonzero areas.
			double area = 0.0;
			const Vector2F& origin = polygon.points[0];
			for (int i = 1; i + 1 < polygon.count; i++)
			{
				const Vector2F& a = polygon.points[i];
				const Vector2F& b = polygon.points[i + 1];
				area += (static_cast<double>(a.x) - origin.x)
					* (static_cast<double>(b.y) - origin.y)
					- (static_cast<double>(a.y) - origin.y)
					* (static_cast<double>(b.x) - origin.x);
			}
			return std::isfinite(area) && area != 0.0;
		}

		// What one candidate axis has to say about the pair.
		struct AxisTest
		{
			bool separated = true;
			// Signed so the caller does not have to guess which way to point
			// the normal: the axis, flipped if `a` is the one further along it.
			Vector2F normal = Vector2F::zero;
			float penetration = 0.0f;
		};

		// What one axis says about two intervals already projected onto it,
		// `a`'s and `b`'s. `axis` must be unit length, or the depths measured
		// on different axes are in different units and the smallest of them
		// means nothing.
		AxisTest test_intervals(float a_min, float a_max, float b_min,
			float b_max, const Vector2F& axis)
		{
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

			// Stated as "both ways out are positive", not as "neither is
			// non-positive". The two agree for real numbers and disagree for
			// NaN, which fails every comparison: the rejecting form fell
			// through to "not separated" and handed back a NaN penetration,
			// which then went into a manifold and out to a resolver as a
			// distance to move something. Written this way, a NaN reports a
			// separating axis, and one separating axis means no contact.
			const bool overlapping = forwards > 0.0f && backwards > 0.0f
				&& std::isfinite(forwards) && std::isfinite(backwards);
			if (!overlapping)
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
				result.normal = -axis;
				result.penetration = forwards;
			}
			return result;
		}

		AxisTest test_axis(const Polygon& a, const Polygon& b, const Vector2F& axis)
		{
			float a_min = 0.0f;
			float a_max = 0.0f;
			float b_min = 0.0f;
			float b_max = 0.0f;
			project(a, axis, a_min, a_max);
			project(b, axis, b_min, b_max);
			return test_intervals(a_min, a_max, b_min, b_max, axis);
		}

		// Whether a polygon can take part in a contact at all: enough points,
		// at least one axis, and an interior.
		bool measurable(const Polygon& polygon)
		{
			return polygon.count >= 3 && polygon.axis_count > 0
				&& has_interior(polygon);
		}

		// The same question for a circle. A radius of zero encloses nothing,
		// and a negative one, which inflate() can produce, is not a shape.
		bool measurable(const Circle& circle)
		{
			const Vector2F center = circle.center();
			return std::isfinite(center.x) && std::isfinite(center.y)
				&& std::isfinite(circle.radius()) && circle.radius() > 0.0f;
		}

		// Two circles overlap along the line between their centres, by as
		// much as the radii exceed that distance. No other axis can be
		// shallower, so this is the whole measurement.
		//
		// Double intermediates keep the difference and its length from
		// overflowing for finite float coordinates, as in fill_axes.
		std::optional<Manifold> circle_circle(const Circle& a, const Circle& b)
		{
			if (!measurable(a) || !measurable(b))
			{
				return std::nullopt;
			}

			const double x = static_cast<double>(b.center().x) - a.center().x;
			const double y = static_cast<double>(b.center().y) - a.center().y;
			const double distance = std::hypot(x, y);
			const double depth =
				static_cast<double>(a.radius()) + b.radius() - distance;
			const float penetration = static_cast<float>(depth);
			if (!(penetration > 0.0f) || !std::isfinite(penetration))
			{
				return std::nullopt;
			}

			Manifold manifold;
			manifold.penetration = penetration;
			// Concentric circles have every direction as a least-penetration
			// axis. Positive x is a fixed choice, so the same pair always
			// separates the same way.
			manifold.normal = distance > 0.0
				? Vector2F(static_cast<float>(x / distance),
					static_cast<float>(y / distance))
				: Vector2F(1.0f, 0.0f);
			return manifold;
		}

		// A circle against a convex polygon, with the normal pointing from
		// the circle towards the polygon.
		//
		// The separating-axis theorem again, with one extra axis: the one
		// from the circle's centre towards the polygon's closest vertex. The
		// polygon's closest point to the centre lies either inside an edge,
		// whose normal is already a candidate, or at a vertex, which is then
		// the closest vertex, and the line from it to the centre is the
		// direction that separates or least penetrates the pair there. The
		// set therefore stays complete and the answer stays exact. A circle
		// projects onto a unit axis as its centre's projection plus and minus
		// its radius.
		std::optional<Manifold> circle_polygon(const Circle& circle,
			const Polygon& polygon)
		{
			if (!measurable(circle) || !measurable(polygon))
			{
				return std::nullopt;
			}

			const Vector2F center = circle.center();
			const float radius = circle.radius();

			Vector2F axes[Polygon::max_points + 1];
			int axis_count = 0;
			for (int i = 0; i < polygon.axis_count; i++)
			{
				axes[axis_count] = polygon.axes[i];
				axis_count++;
			}

			int closest = 0;
			double closest_distance = std::numeric_limits<double>::infinity();
			for (int i = 0; i < polygon.count; i++)
			{
				const double x = static_cast<double>(polygon.points[i].x) - center.x;
				const double y = static_cast<double>(polygon.points[i].y) - center.y;
				const double distance = std::hypot(x, y);
				if (distance < closest_distance)
				{
					closest = i;
					closest_distance = distance;
				}
			}

			// A centre exactly on a vertex has no direction to it, and needs
			// none: that vertex is on the boundary, so an edge normal already
			// measures the least penetration.
			if (closest_distance > 0.0 && std::isfinite(closest_distance))
			{
				const double x =
					static_cast<double>(polygon.points[closest].x) - center.x;
				const double y =
					static_cast<double>(polygon.points[closest].y) - center.y;
				axes[axis_count] = Vector2F(
					static_cast<float>(x / closest_distance),
					static_cast<float>(y / closest_distance));
				axis_count++;
			}

			Manifold least;
			least.penetration = std::numeric_limits<float>::max();
			for (int i = 0; i < axis_count; i++)
			{
				const float projected = Vector2F::dot(center, axes[i]);
				float polygon_min = 0.0f;
				float polygon_max = 0.0f;
				project(polygon, axes[i], polygon_min, polygon_max);
				const AxisTest test = test_intervals(projected - radius,
					projected + radius, polygon_min, polygon_max, axes[i]);
				if (test.separated)
				{
					return std::nullopt;
				}

				if (test.penetration < least.penetration)
				{
					least.normal = test.normal;
					least.penetration = test.penetration;
				}
			}

			if (least.normal == Vector2F::zero)
			{
				return std::nullopt;
			}

			return least;
		}

		std::optional<Manifold> polygon_polygon(const Polygon& polygon_a,
			const Polygon& polygon_b)
		{
			if (!measurable(polygon_a) || !measurable(polygon_b))
			{
				// Nothing can overlap a shape that encloses no area, and a shape
				// with no axes has no edges worth testing.
				return std::nullopt;
			}

			Manifold least;
			least.penetration = std::numeric_limits<float>::max();

			const Polygon* const polygons[2] = { &polygon_a, &polygon_b };
			for (const Polygon* polygon : polygons)
			{
				for (int i = 0; i < polygon->axis_count; i++)
				{
					const AxisTest test =
						test_axis(polygon_a, polygon_b, polygon->axes[i]);
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

			if (least.normal == Vector2F::zero)
			{
				// Every edge was degenerate, so neither shape has an interior.
				return std::nullopt;
			}

			return least;
		}
	}

	std::optional<Manifold> narrow_phase(const Shape& a, const Shape& b)
	{
		// The downcasts are safe for the reason polygon_from's are.
		const bool a_is_circle = a.shape_type() == ShapeType::circle;
		const bool b_is_circle = b.shape_type() == ShapeType::circle;
		if (a_is_circle && b_is_circle)
		{
			return circle_circle(static_cast<const Circle&>(a),
				static_cast<const Circle&>(b));
		}
		if (a_is_circle)
		{
			return circle_polygon(static_cast<const Circle&>(a), polygon_from(b));
		}
		if (b_is_circle)
		{
			// Measured from the circle's side, then turned round so the
			// normal still points from `a` towards `b`.
			std::optional<Manifold> manifold =
				circle_polygon(static_cast<const Circle&>(b), polygon_from(a));
			if (manifold.has_value())
			{
				manifold->normal = -manifold->normal;
			}
			return manifold;
		}
		return polygon_polygon(polygon_from(a), polygon_from(b));
	}
}

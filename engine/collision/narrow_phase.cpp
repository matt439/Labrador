#include "engine/collision/narrow_phase.h"

#include "engine/math/ericson_math.h"

#include <algorithm>
#include <limits>
#include <span>
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
			Vector2F axes[MAX_POINTS];
			int axis_count = 0;
		};

		// The shortest edge that still says something about direction.
		//
		// An edge is a subtraction of two nearby coordinates, so its absolute
		// error is set by where it is, not by how long it is: out at 6200,
		// where consecutive floats are 4.88e-4 apart, an edge a thousandth of
		// a unit long is built almost entirely from the bits that subtraction
		// destroyed. Normalising it produces a confident unit vector pointing
		// in a direction nothing measured.
		//
		// A thousandth of a world unit is also far below anything the content
		// contains - level geometry is authored in multiples of five - so
		// nothing real is discarded here.
		constexpr float MIN_EDGE_LENGTH = 0.001f;

		// Fills `axes` from the first `edge_count` edges. Degenerate edges
		// contribute nothing.
		void fill_axes(Polygon& polygon, int edge_count)
		{
			for (int i = 0; i < edge_count; i++)
			{
				const Vector2F edge =
					polygon.points[(i + 1) % polygon.count] - polygon.points[i];

				// Tested on the edge, before normalising, and not on the
				// result afterwards. normalized() returns zero only when the
				// length is exactly 0.0f, so an edge of 1e-7 passed that guard
				// and contributed an axis whose direction was noise - and a
				// bogus axis either falsely separates the pair or wins the
				// least-penetration contest with a meaningless normal.
				if (Vector2F::dot(edge, edge) < MIN_EDGE_LENGTH * MIN_EDGE_LENGTH)
				{
					continue;
				}

				const Vector2F direction = edge.normalized();

				polygon.axes[polygon.axis_count] =
					Vector2F(-direction.y, direction.x);
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
				const auto& triangle = static_cast<const mattmath::Triangle&>(shape);
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
				const auto& quad = static_cast<const mattmath::Quad&>(shape);
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
				// Read straight off the shape. This used to build a
				// mattmath::Quad from it, inside the one function whose
				// comment above boasts about avoiding a heap allocation per
				// shape per pair per frame - and that constructor allocates
				// and validates twice over.
				const auto& rotated =
					static_cast<const mattmath::RectangleRotated&>(shape);
				polygon.points[0] = rotated.point_0();
				polygon.points[1] = rotated.point_1();
				polygon.points[2] = rotated.point_2();
				polygon.points[3] = rotated.point_3();
				polygon.count = 4;

				// Still a rectangle, so still two distinct axes - they are
				// simply no longer the cardinal ones.
				fill_axes(polygon, 2);
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
		// handles it correctly. mattmath::Triangle permits a collinear one -
		// its constructor validates nothing - so this is caught here rather
		// than being assumed away.
		bool has_interior(const Polygon& polygon)
		{
			const std::span<const mattmath::Point2F> points(
				polygon.points, static_cast<size_t>(polygon.count));

			return mattmath::signed_area(points) != 0.0f;
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

			// Stated as "both ways out are positive", not as "neither is
			// non-positive". The two agree for real numbers and disagree for
			// NaN, which fails every comparison: the rejecting form fell
			// through to "not separated" and handed back a NaN penetration,
			// which then went into a manifold and out to a resolver as a
			// distance to move something. Written this way, a NaN reports a
			// separating axis, and one separating axis means no contact.
			const bool overlapping = forwards > 0.0f && backwards > 0.0f;
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

		if (polygon_a.count < 3 || polygon_b.count < 3
			|| polygon_a.axis_count == 0 || polygon_b.axis_count == 0
			|| !has_interior(polygon_a) || !has_interior(polygon_b))
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

		if (least.normal == Vector2F::ZERO)
		{
			// Every edge was degenerate, so neither shape has an interior.
			return std::nullopt;
		}

		return least;
	}
}

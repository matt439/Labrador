#include <doctest/doctest.h>

#include "engine/math/intersects.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/scalar.h"
#include "engine/math/segment.h"
#include "engine/math/vector2f.h"

#include <stdexcept>

using namespace mattmath;

TEST_SUITE("RectangleRotated")
{
	TEST_CASE("a rotated rectangle can actually be rotated")
	{
		// set_x_axis validates against the unchanged y_axis_ and
		// set_y_axis against the unchanged x_axis_, so a genuine rotation
		// was rejected whichever was called first - the rectangle could
		// not be turned through its own API at all. Both single-axis
		// setters still refuse, correctly; set_axes is the one that turns.
		RectangleRotated r(Point2F(0.0f, 0.0f), Vector2F::DIRECTION_RIGHT,
			Vector2F::DIRECTION_DOWN, Vector2F(10.0f, 5.0f));

		const float turn = PI / 6.0f;
		const Vector2F x = Vector2F::unit_vec_from_angle(turn);

		// The old route, both orders, still throws - that is what it is
		// for and it is not what is being fixed.
		CHECK_THROWS_AS(r.set_x_axis(x), std::invalid_argument);
		CHECK_THROWS_AS(r.set_y_axis(Vector2F::normal(x)),
			std::invalid_argument);

		r.set_axes(x, Vector2F::normal(x));

		CHECK(are_equal(r.angle(), turn));
		CHECK(r.is_valid());

		// A non-orthogonal pair is refused, and refused transactionally.
		const RectangleRotated before = r;
		CHECK_THROWS_AS(r.set_axes(Vector2F::DIRECTION_RIGHT,
			Vector2F(1.0f, 1.0f)), std::invalid_argument);
		CHECK(r == before);
	}
	TEST_CASE("a degenerate rotated rectangle answers contains(), it does not throw")
	{
		// contains() reached Quad's constructor, which rejects four
		// coincident points - so a bool predicate on a default-constructed
		// RectangleRotated threw invalid_argument("Quad is not valid"),
		// naming a type the caller never mentioned. A shape with no
		// interior contains nothing, which is what every other degenerate
		// shape in the library now says.
		const RectangleRotated degenerate;

		CHECK_NOTHROW(point_rectangle_rotated_intersect(Point2F(1.0f, 1.0f), degenerate));
		CHECK_FALSE(point_rectangle_rotated_intersect(Point2F(1.0f, 1.0f), degenerate));
		CHECK_FALSE(point_rectangle_rotated_intersect(Point2F::ZERO, degenerate));

		// A real one is unaffected, boundary included.
		const RectangleRotated real(Point2F(0.0f, 0.0f),
			Vector2F::DIRECTION_RIGHT, Vector2F::DIRECTION_DOWN,
			Vector2F(10.0f, 5.0f));

		CHECK(point_rectangle_rotated_intersect(Point2F::ZERO, real));
		CHECK(point_rectangle_rotated_intersect(Point2F(9.9f, 4.9f), real));
		CHECK(point_rectangle_rotated_intersect(Point2F(10.0f, 5.0f), real));
		CHECK_FALSE(point_rectangle_rotated_intersect(Point2F(10.1f, 0.0f), real));
	}
	TEST_CASE("a rotated rectangle's angle keeps its sign")
	{
		// angle_between is an acos, so its range is [0, PI] and a
		// rectangle turned one way reported the same number as its mirror
		// image. Nothing could rebuild the orientation from it - which is
		// the likeliest reason the DrawObject setter that consumes it was
		// never written.
		const Vector2F hw(10.0f, 5.0f);

		const float turn = PI / 6.0f;
		const Vector2F x_pos = Vector2F::unit_vec_from_angle(turn);
		const RectangleRotated positive(Point2F(0.0f, 0.0f), x_pos,
			Vector2F::normal(x_pos), hw);

		const Vector2F x_neg = Vector2F::unit_vec_from_angle(-turn);
		const RectangleRotated negative(Point2F(0.0f, 0.0f), x_neg,
			Vector2F::normal(x_neg), hw);

		CHECK(are_equal(positive.angle(), turn));
		CHECK(are_equal(negative.angle(), -turn));
		CHECK(positive.angle() != negative.angle());

		// An unrotated rectangle is still zero.
		const RectangleRotated flat(Point2F(0.0f, 0.0f),
			Vector2F::DIRECTION_RIGHT, Vector2F::DIRECTION_DOWN, hw);

		CHECK(are_equal(flat.angle(), 0.0f));
	}
	TEST_CASE("test_rectangle_rotated_segment_constructor")
	{
		// The (Segment, thickness) overload is what paint trails and thick
		// line segments use. Segment::direction() is un-normalised, so
		// this used to throw for every segment not exactly 1 unit long.
		const float thickness = 2.0f;
		Segment s(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));
		RectangleRotated rr(s, thickness);

		CHECK(rr.is_valid());

		// centre is the segment midpoint
		CHECK(are_equal(rr.center().x, 5.0f));
		CHECK(are_equal(rr.center().y, 0.0f));

		// x axis runs along the segment and is a unit vector
		CHECK(are_equal(rr.x_axis().length(), 1.0f));
		CHECK(are_equal(rr.y_axis().length(), 1.0f));
		CHECK(are_equal(rr.x_axis().x, 1.0f));
		CHECK(are_equal(rr.x_axis().y, 0.0f));

		// half extents: half the length plus the thickness along the
		// segment, the thickness across it
		CHECK(are_equal(rr.half_x_width(),
			5.0f + thickness));
		CHECK(are_equal(rr.half_y_width(),
			thickness));

		// a diagonal segment stays valid and picks up the right angle
		Segment diagonal(Point2F(0.0f, 0.0f), Point2F(10.0f, 10.0f));
		RectangleRotated rr_diagonal(diagonal, 1.0f);
		CHECK(rr_diagonal.is_valid());
		CHECK(are_equal(rr_diagonal.x_axis().length(),
			1.0f));
	}
	TEST_CASE("test_rectangle_rotated_rejects_invalid")
	{
		// is_valid() consults edges_valid(), which reads points_. When
		// points_ was still all-zero at validation time these degenerate
		// cases were accepted.

		// zero-length segment: no direction, so no valid axes
		Segment degenerate(Point2F(3.0f, 3.0f), Point2F(3.0f, 3.0f));
		CHECK_THROWS_AS([&] { RectangleRotated rr(degenerate, 1.0f); }(), std::invalid_argument);

		// zero half extents
		CHECK_THROWS_AS([&] { RectangleRotated rr(Point2F::ZERO,
				Vector2F::DIRECTION_RIGHT, Vector2F::DIRECTION_DOWN,
				Vector2F::ZERO); }(), std::invalid_argument);

		// non-perpendicular axes
		CHECK_THROWS_AS([&] { RectangleRotated rr(Point2F::ZERO,
				Vector2F::DIRECTION_RIGHT, Vector2F::DIRECTION_DOWN_RIGHT,
				Vector2F(5.0f, 5.0f)); }(), std::invalid_argument);
	}
	TEST_CASE("test_rectangle_rotated_setters_are_transactional")
	{
		// A rejected setter argument must leave the rectangle untouched.
		RectangleRotated rr(Point2F::ZERO, Vector2F::DIRECTION_RIGHT,
			Vector2F::DIRECTION_DOWN, Vector2F(5.0f, 5.0f));

		CHECK_THROWS_AS([&] { rr.set_half_x_width(-1.0f); }(), std::invalid_argument);
		CHECK(are_equal(rr.half_x_width(), 5.0f));
		CHECK(rr.is_valid());

		CHECK_THROWS_AS([&] { rr.set_half_extents(Vector2F(0.0f, 0.0f)); }(), std::invalid_argument);
		CHECK(are_equal(rr.half_x_width(), 5.0f));
		CHECK(are_equal(rr.half_y_width(), 5.0f));

		CHECK_THROWS_AS([&] { rr.set_x_axis(Vector2F::DIRECTION_DOWN_RIGHT); }(), std::invalid_argument);
		CHECK(are_equal(rr.x_axis().x, 1.0f));
		CHECK(rr.is_valid());

		CHECK_THROWS_AS([&] { rr.inflate(-10.0f); }(), std::invalid_argument);
		CHECK(are_equal(rr.half_x_width(), 5.0f));
		CHECK(rr.is_valid());

		// a valid inflate still works, and keeps the corner cache in sync
		rr.inflate(1.0f);
		CHECK(are_equal(rr.half_x_width(), 6.0f));
		CHECK(are_equal(rr.point_2().x, 6.0f));
		CHECK(rr.is_valid());
	}
}

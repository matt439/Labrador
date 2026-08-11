#include <doctest/doctest.h>

#include "engine/math/matt_math.h"
#include "engine/math/ericson_math.h"

#include <array>
#include <cfloat>
#include <cmath>
#include <limits>
#include <span>

using namespace mattmath;

TEST_SUITE("EricsonMath")
{
	TEST_CASE("test_closest_pt_point_AABB")
	{
		RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);
		Point2F p, closest;

		// p is inside a
		p = Point2F(5.0f, 5.0f);
		closest_pt_point_AABB(p, a, closest);
		CHECK(closest == p);

		// p is outside a
		p = Point2F(15.0f, 15.0f);
		closest_pt_point_AABB(p, a, closest);
		CHECK(closest == Point2F(10.0f, 10.0f));

		// p is outside a
		p = Point2F(-5.0f, -5.0f);
		closest_pt_point_AABB(p, a, closest);
		CHECK(closest == Point2F(0.0f, 0.0f));

		// p is outside a
		p = Point2F(5.0f, 15.0f);
		closest_pt_point_AABB(p, a, closest);
		CHECK(closest == Point2F(5.0f, 10.0f));

		// p is just inside a
		p = Point2F(10.0f - FLT_EPSILON, 10.0f - FLT_EPSILON);
		closest_pt_point_AABB(p, a, closest);
		CHECK(closest == p);

		// p is just outside a
		p = Point2F(10.0f + FLT_EPSILON, 10.0f + FLT_EPSILON);
		closest_pt_point_AABB(p, a, closest);
		CHECK(closest == Point2F(10.0f, 10.0f));
	}
	TEST_CASE("signed_area measures a polygon, and its sign is the winding")
	{
		const std::array<Point2F, 4> square = {
			Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f) };
		const std::array<Point2F, 4> reversed = {
			Point2F(0.0f, 10.0f), Point2F(10.0f, 10.0f),
			Point2F(10.0f, 0.0f), Point2F(0.0f, 0.0f) };

		CHECK(signed_area(square) == 100.0f);
		CHECK(signed_area(reversed) == -100.0f);

		// The polygon closes itself; the caller does not repeat the first
		// vertex at the end.
		const std::array<Point2F, 3> triangle = {
			Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f) };
		CHECK(signed_area(triangle) == 50.0f);

		// Which is half of what the triangle predicate reports, since that
		// one returns twice the area.
		CHECK(signed_area(triangle) ==
			signed_2D_tri_area(triangle[0], triangle[1], triangle[2]) / 2.0f);
	}
	TEST_CASE("signed_area is zero for everything that encloses nothing")
	{
		const std::array<Point2F, 2> segment = {
			Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f) };
		CHECK(signed_area(segment) == 0.0f);
		CHECK(signed_area(std::span<const Point2F>{}) == 0.0f);

		// Collinear, and a repeated vertex.
		const std::array<Point2F, 3> flat = {
			Point2F(0.0f, 0.0f), Point2F(5.0f, 0.0f), Point2F(10.0f, 0.0f) };
		const std::array<Point2F, 3> doubled = {
			Point2F(3.0f, 3.0f), Point2F(3.0f, 3.0f), Point2F(9.0f, 9.0f) };
		CHECK(signed_area(flat) == 0.0f);
		CHECK(signed_area(doubled) == 0.0f);
	}
	TEST_CASE("signed_area stays exact at the coordinates the game runs at")
	{
		// Out where the levels are, the raw coordinates form products near
		// 3e7, and consecutive floats up there are 2 apart. Summing four
		// of those and cancelling down to the area of a small shape leaves
		// an error set by the size of the world rather than the size of
		// the shape - this square comes out as 176.0 computed that way,
		// against a true 176.88. Taken relative to the first vertex, every
		// product is the size of the polygon and the answer is exactly the
		// width times the height.
		//
		// Round coordinates hide this: a 40-unit tile at (6200, 5000) has
		// products that are all exactly representable, so both
		// formulations agree and prove nothing.
		constexpr float X = 6232.75f;
		constexpr float Y = 5408.46f;
		constexpr float SIDE = 13.3f;

		const std::array<Point2F, 4> tile = {
			Point2F(X, Y), Point2F(X + SIDE, Y),
			Point2F(X + SIDE, Y + SIDE), Point2F(X, Y + SIDE) };

		// The side lengths the corners actually store, once each sum has
		// been rounded to a float.
		const float width = tile[1].x - tile[0].x;
		const float height = tile[2].y - tile[1].y;

		CHECK(signed_area(tile) == width * height);
		CHECK(signed_area(tile) != 176.0f);
	}
	TEST_CASE("signed_area of a quad agrees with the cross of its diagonals")
	{
		// Ericson notes the quad case collapses to one cross product of
		// the diagonals rather than a term per edge. The identity is real
		// but the direction matters: it is cross(A - C, B - D), and
		// cross(C - A, B - D) gives twice the area negated.
		const std::array<Point2F, 4> quad = {
			Point2F(1.0f, 2.0f), Point2F(9.0f, 1.0f),
			Point2F(11.0f, 7.0f), Point2F(2.0f, 8.0f) };

		const float twice = 2.0f * signed_area(quad);

		CHECK(twice == Vector2F::cross(quad[0] - quad[2], quad[1] - quad[3]));
		CHECK(-twice == Vector2F::cross(quad[2] - quad[0], quad[1] - quad[3]));
	}
	TEST_CASE("point_in_convex_polygon takes the boundary as inside")
	{
		const std::array<Point2F, 4> square = {
			Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f) };

		CHECK(point_in_convex_polygon(square, Point2F(5.0f, 5.0f)));
		CHECK(point_in_convex_polygon(square, Point2F(5.0f, 0.0f)));
		CHECK(point_in_convex_polygon(square, Point2F(0.0f, 0.0f)));

		CHECK_FALSE(point_in_convex_polygon(square, Point2F(15.0f, 5.0f)));
		CHECK_FALSE(point_in_convex_polygon(square, Point2F(-1.0f, 5.0f)));

		// Diagonally past a corner: outside, though it is beyond only one
		// of the two edges that meet there by a whole unit.
		CHECK_FALSE(point_in_convex_polygon(square, Point2F(11.0f, 11.0f)));
	}
	TEST_CASE("point_in_convex_polygon does not care which way the caller wound it")
	{
		const std::array<Point2F, 3> forwards = {
			Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f) };
		const std::array<Point2F, 3> backwards = {
			Point2F(0.0f, 10.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 0.0f) };

		const Point2F inside(2.0f, 2.0f);
		const Point2F outside(9.0f, 9.0f);

		CHECK(point_in_convex_polygon(forwards, inside));
		CHECK(point_in_convex_polygon(backwards, inside));
		CHECK_FALSE(point_in_convex_polygon(forwards, outside));
		CHECK_FALSE(point_in_convex_polygon(backwards, outside));
	}
	TEST_CASE("point_in_convex_polygon rejects what encloses nothing")
	{
		const std::array<Point2F, 2> segment = {
			Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f) };

		CHECK_FALSE(point_in_convex_polygon(segment, Point2F(5.0f, 0.0f)));
		CHECK_FALSE(point_in_convex_polygon(std::span<const Point2F>{},
			Point2F(0.0f, 0.0f)));
	}
	TEST_CASE("a zero-length segment is its own closest point, not a NaN")
	{
		// Reachable: circle_segment_intersect forwards straight into this,
		// with edges pulled from a shape. The old form divided by the
		// segment's zero length first, and NaN fails every comparison, so
		// both clamps declined to catch it and the caller compared
		// NaN <= radius - false - and reported no intersection.
		const Point2F point(3.0f, 4.0f);
		const Point2F degenerate(10.0f, 10.0f);

		float t = -1.0f;
		Point2F closest;
		closest_pt_point_segment(point, degenerate, degenerate, t, closest);

		CHECK(t == 0.0f);
		CHECK(closest == degenerate);
		CHECK_FALSE(std::isnan(closest.x));
		CHECK_FALSE(std::isnan(closest.y));
	}
	TEST_CASE("a collinear triangle contains nothing, and says so")
	{
		// This case has had three answers and only the third is the one
		// the title claims. The barycentric form divided by the
		// triangle's determinant, zero here, and every comparison against
		// the resulting NaN was false - "outside" for the whole plane, by
		// accident. The sign form that replaced it accepted every point
		// whose signed area against all three edges was zero, which is the
		// entire infinite supporting line, and the point at (2, 0) below
		// was pinned as INSIDE while the title said otherwise.
		//
		// A shape with no area encloses nothing, which is what
		// signed_area says about these same three points. So: nothing is
		// inside, on the line or off it, near the vertices or a thousand
		// units past them.
		const Point2F a(0.0f, 0.0f);
		const Point2F b(5.0f, 0.0f);
		const Point2F c(10.0f, 0.0f);

		CHECK_FALSE(test_point_triangle(Point2F(2.0f, 0.0f), a, b, c));
		CHECK_FALSE(test_point_triangle(Point2F(2.0f, 3.0f), a, b, c));

		// On the supporting line but far outside the vertices. This is the
		// one the sign form got most visibly wrong.
		CHECK_FALSE(test_point_triangle(Point2F(-1000.0f, 0.0f), a, b, c));

		// A vertex of a shape that has none.
		CHECK_FALSE(test_point_triangle(a, a, b, c));
	}
	TEST_CASE("a triangle with two coincident vertices has a closest point, not a NaN")
	{
		// The sibling of "a zero-length segment is its own closest point,
		// not a NaN", and the same arithmetic: the AB edge region divides
		// by dot(ab, ab), which is zero when a and b are the same point.
		// The result travelled - test_circle_triangle returns it as the
		// contact point on the branch that reports a hit, so a caller
		// asking where two things touched was handed (NaN, NaN) while
		// being told they did.
		const Point2F a(0.0f, 0.0f);
		const Point2F b(0.0f, 0.0f);
		const Point2F c(10.0f, 0.0f);

		// Not merely finite: correct. With a and b coincident the shape is
		// the segment a-c, so the closest point to (5, 1) is (5, 0).
		// Returning a instead would also be NaN-free and would be wrong by
		// five units.
		const Point2F closest = closest_pt_point_triangle(
			Point2F(5.0f, 1.0f), a, b, c);

		CHECK_FALSE(std::isnan(closest.x));
		CHECK_FALSE(std::isnan(closest.y));
		CHECK(closest == Point2F(5.0f, 0.0f));

		// Beyond each end, where the vertex regions answer.
		CHECK(closest_pt_point_triangle(Point2F(-4.0f, 3.0f), a, b, c) == a);
		CHECK(closest_pt_point_triangle(Point2F(14.0f, 3.0f), a, b, c) == c);

		// All three vertices the same, approached from anywhere.
		const Point2F degenerate = closest_pt_point_triangle(
			Point2F(3.0f, 4.0f), a, b, a);

		CHECK_FALSE(std::isnan(degenerate.x));
		CHECK_FALSE(std::isnan(degenerate.y));
		CHECK(degenerate == a);

		// And the circle test that consumes it now reports a usable point
		// rather than a poisoned one.
		Point2F contact;
		const Circle circle(Point2F(5.0f, 1.0f), 5.0f);

		CHECK(test_circle_triangle(circle, a, b, c, contact));
		CHECK_FALSE(std::isnan(contact.x));
		CHECK_FALSE(std::isnan(contact.y));
	}
	TEST_CASE("a NaN coordinate is inside nothing and hits nothing")
	{
		// The companion to "a NaN coordinate makes test_AABB_AABB report
		// no intersection". All three predicates now answer a poisoned
		// input the same way; two of them used to answer "yes".
		const float nan = std::numeric_limits<float>::quiet_NaN();

		const Point2F polygon[4] = { Point2F(0.0f, 0.0f),
			Point2F(10.0f, 0.0f), Point2F(10.0f, 10.0f),
			Point2F(0.0f, 10.0f) };

		CHECK_FALSE(point_in_convex_polygon(polygon, Point2F(nan, 5.0f)));
		CHECK_FALSE(point_in_convex_polygon(polygon, Point2F(5.0f, nan)));

		// A poisoned vertex, rather than a poisoned query point.
		const Point2F poisoned[4] = { Point2F(nan, 0.0f),
			Point2F(10.0f, 0.0f), Point2F(10.0f, 10.0f),
			Point2F(0.0f, 10.0f) };

		CHECK_FALSE(point_in_convex_polygon(poisoned, Point2F(5.0f, 5.0f)));

		const RectangleF box(0.0f, 0.0f, 10.0f, 10.0f);

		CHECK_FALSE(test_segment_AABB(Point2F(nan, 5.0f),
			Point2F(20.0f, 5.0f), box));
		CHECK_FALSE(test_segment_AABB(Point2F(-20.0f, 5.0f),
			Point2F(20.0f, nan), box));
		CHECK_FALSE(test_segment_AABB(Point2F(-20.0f, 5.0f),
			Point2F(20.0f, 5.0f), RectangleF(nan, 0.0f, 10.0f, 10.0f)));

		// The segment that does cross it is unaffected.
		CHECK(test_segment_AABB(Point2F(-20.0f, 5.0f),
			Point2F(20.0f, 5.0f), box));
	}
	TEST_CASE("test_point_triangle does not depend on the winding")
	{
		const Point2F a(0.0f, 0.0f);
		const Point2F b(10.0f, 0.0f);
		const Point2F c(0.0f, 10.0f);
		const Point2F inside(2.0f, 2.0f);
		const Point2F outside(9.0f, 9.0f);

		CHECK(test_point_triangle(inside, a, b, c));
		CHECK(test_point_triangle(inside, a, c, b));
		CHECK_FALSE(test_point_triangle(outside, a, b, c));
		CHECK_FALSE(test_point_triangle(outside, a, c, b));

		// The boundary is inside, which is what the barycentric form's
		// v >= 0 && w >= 0 && v + w <= 1 also said.
		CHECK(test_point_triangle(Point2F(5.0f, 0.0f), a, b, c));
		CHECK(test_point_triangle(a, a, b, c));
	}
	TEST_CASE("a NaN coordinate makes test_AABB_AABB report no intersection")
	{
		// It used to report the opposite. The rejecting form - "return
		// false if separated on either axis, otherwise true" - reaches its
		// accept branch by falling through, and NaN fails every
		// comparison, so a single bad coordinate produced a box that
		// intersected everything in the level. This routine is live behind
		// Level::is_object_out_of_bounds.
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const RectangleF good(0.0f, 0.0f, 10.0f, 10.0f);
		const RectangleF poisoned(nan, 0.0f, 10.0f, 10.0f);

		CHECK_FALSE(test_AABB_AABB(poisoned, good));
		CHECK_FALSE(test_AABB_AABB(good, poisoned));

		// Ordinary answers are unchanged, including the closed boundary
		// that contacts.cpp depends on: boxes sharing only an edge still
		// intersect.
		CHECK(test_AABB_AABB(good, RectangleF(5.0f, 5.0f, 10.0f, 10.0f)));
		CHECK(test_AABB_AABB(good, RectangleF(10.0f, 0.0f, 10.0f, 10.0f)));
		CHECK_FALSE(test_AABB_AABB(good, RectangleF(11.0f, 0.0f, 10.0f, 10.0f)));
	}
	TEST_CASE("test_signed_2D_tri_area")
	{
		Point2F a(0.0f, 0.0f);
		Point2F b(10.0f, 0.0f);
		Point2F c(0.0f, 10.0f);

		// a, b, c are in counter-clockwise order
		CHECK(signed_2D_tri_area(a, b, c) == 100.0f);

		// a, b, c are in clockwise order
		CHECK(signed_2D_tri_area(a, c, b) == -100.0f);

		// a, b, c are collinear
		CHECK(signed_2D_tri_area(a, b, Point2F(20.0f, 0.0f)) == 0.0f);
	}
	TEST_CASE("test_test_2D_segment_segment")
	{
		float t;
		Point2F p;
		Segment s1(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));

		// s1 and s2 intersect
		Segment s2(Point2F(5.0f, -5.0f), Point2F(5.0f, 5.0f));
		CHECK(test_2D_segment_segment(s1.point_0, s1.point_1,
			s2.point_0, s2.point_1, t, p));
		CHECK(p == Point2F(5.0f, 0.0f));
		CHECK(t == 0.5f);

		// s1 and s2 not intersecting
		s2 = Segment(Point2F(15.0f, -5.0f), Point2F(15.0f, 5.0f));
		CHECK_FALSE(test_2D_segment_segment(s1.point_0, s1.point_1,
			s2.point_0, s2.point_1, t, p));
	}
	TEST_CASE("test_closest_pt_point_triangle")
	{
		Triangle t(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f));
		Point2F t0 = t.point_0();
		Point2F t1 = t.point_1();
		Point2F t2 = t.point_2();
		Point2F closest;

		Point2F p = Point2F(5.0f, 5.0f);
		CHECK(closest_pt_point_triangle(p, t0, t1, t2) == p);

		p = Point2F(15.0f, 15.0f);
		closest = closest_pt_point_triangle(p, t0, t1, t2);
		CHECK(closest == Point2F(5.0f, 5.0f));

		p = Point2F(-5.0f, -5.0f);
		closest = closest_pt_point_triangle(p, t0, t1, t2);
		CHECK(closest == Point2F(0.0f, 0.0f));

		p = Point2F(5.0f, -5.0f);
		closest = closest_pt_point_triangle(p, t0, t1, t2);
		CHECK(closest == Point2F(5.0f, 0.0f));
	}
	TEST_CASE("test_closest_pt_point_segment")
	{
		Segment s(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));
		Point2F s0 = s.point_0;
		Point2F s1 = s.point_1;
		Point2F closest;
		float t;

		Point2F p = Point2F(5.0f, 5.0f);
		closest_pt_point_segment(p, s0, s1, t, closest);
		CHECK(closest == Point2F(5.0f, 0.0f));
		CHECK(t == 0.5f);

		p = Point2F(15.0f, 5.0f);
		closest_pt_point_segment(p, s0, s1, t, closest);
		CHECK(closest == Point2F(10.0f, 0.0f));
		CHECK(t == 1.0f);

		p = Point2F(-5.0f, 5.0f);
		closest_pt_point_segment(p, s0, s1, t, closest);
		CHECK(closest == Point2F(0.0f, 0.0f));
		CHECK(t == 0.0f);

		p = Point2F(5.0f, -5.0f);
		closest_pt_point_segment(p, s0, s1, t, closest);
		CHECK(closest == Point2F(5.0f, 0.0f));
		CHECK(t == 0.5f);
	}
	TEST_CASE("test_closest_pt_point_OBB_axis_aligned")
	{
		// An axis-aligned OBB spanning (-5,-5) to (5,5), so every answer
		// can be checked against the AABB result.
		OBB b(Point2F::ZERO, Vector2F::DIRECTION_RIGHT,
			Vector2F::DIRECTION_DOWN, Vector2F(5.0f, 5.0f));
		Point2F q;

		// inside the box: the closest point is the point itself
		closest_pt_point_OBB(Point2F(1.0f, 2.0f), b, q);
		CHECK(q == Point2F(1.0f, 2.0f));

		// outside along +x only: clamps on x, keeps y
		closest_pt_point_OBB(Point2F(10.0f, 2.0f), b, q);
		CHECK(q == Point2F(5.0f, 2.0f));

		// outside along -y only
		closest_pt_point_OBB(Point2F(0.0f, -10.0f), b, q);
		CHECK(q == Point2F(0.0f, -5.0f));

		// past a corner: clamps on both axes
		closest_pt_point_OBB(Point2F(10.0f, 10.0f), b, q);
		CHECK(q == Point2F(5.0f, 5.0f));

		// exactly on a face
		closest_pt_point_OBB(Point2F(5.0f, 0.0f), b, q);
		CHECK(q == Point2F(5.0f, 0.0f));
	}
	TEST_CASE("test_closest_pt_point_OBB_rotated")
	{
		// A square rotated 45 degrees - a diamond with vertices at
		// (+-5*sqrt2, 0) and (0, +-5*sqrt2). Here the OBB answer differs
		// from the AABB answer, which is the whole point of the routine.
		OBB b(Point2F::ZERO, Vector2F::DIRECTION_DOWN_RIGHT,
			Vector2F::DIRECTION_DOWN_LEFT, Vector2F(5.0f, 5.0f));
		Point2F q;

		const float half_diagonal = 5.0f * std::sqrt(2.0f);

		// inside
		closest_pt_point_OBB(Point2F(1.0f, 1.0f), b, q);
		CHECK(are_equal(q.x, 1.0f));
		CHECK(are_equal(q.y, 1.0f));

		// straight out along +x lands on the vertex
		closest_pt_point_OBB(Point2F(20.0f, 0.0f), b, q);
		CHECK(are_equal(q.x, half_diagonal));
		CHECK(are_equal(q.y, 0.0f));

		// out along the diagonal lands on an edge midpoint, NOT on the
		// bounding-box corner (half_diagonal, half_diagonal)
		closest_pt_point_OBB(Point2F(10.0f, 10.0f), b, q);
		CHECK(are_equal(q.x, half_diagonal / 2.0f));
		CHECK(are_equal(q.y, half_diagonal / 2.0f));
	}
}

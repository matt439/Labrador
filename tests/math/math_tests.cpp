#include <doctest/doctest.h>
#include "engine/math/matt_math.h"
#include "engine/math/ericson_math.h"
#include <array>
#include <cfloat>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>

using namespace mattmath;

namespace MathTestConstants
{
	constexpr float EPSILON_F = 0.000001f;
	constexpr float EPSILON_F_2 = 0.000002f;
	constexpr float EPSILON_F_100 = 0.0001f;
}

using namespace MathTestConstants;

namespace EricsonMathTests
{
	TEST_SUITE("EricsonMathTests")
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
			// The barycentric form divided by the triangle's determinant,
			// which is zero here, so every comparison against the resulting
			// NaN was false and the answer was "outside" for every point in
			// the plane - including the ones lying on the degenerate triangle
			// itself. Now the point on it is inside and the one off it is not.
			const Point2F a(0.0f, 0.0f);
			const Point2F b(5.0f, 0.0f);
			const Point2F c(10.0f, 0.0f);

			CHECK(test_point_triangle(Point2F(2.0f, 0.0f), a, b, c));
			CHECK_FALSE(test_point_triangle(Point2F(2.0f, 3.0f), a, b, c));
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
		TEST_CASE("test_test_segment_AABB")
		{
			// tested below with rectangle_segment_intersect
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
			CHECK(are_equal(q.x, 1.0f, EPSILON_F_100));
			CHECK(are_equal(q.y, 1.0f, EPSILON_F_100));

			// straight out along +x lands on the vertex
			closest_pt_point_OBB(Point2F(20.0f, 0.0f), b, q);
			CHECK(are_equal(q.x, half_diagonal, EPSILON_F_100));
			CHECK(are_equal(q.y, 0.0f, EPSILON_F_100));

			// out along the diagonal lands on an edge midpoint, NOT on the
			// bounding-box corner (half_diagonal, half_diagonal)
			closest_pt_point_OBB(Point2F(10.0f, 10.0f), b, q);
			CHECK(are_equal(q.x, half_diagonal / 2.0f, EPSILON_F_100));
			CHECK(are_equal(q.y, half_diagonal / 2.0f, EPSILON_F_100));
		}
	};
} // namespace EricsonMathTests

namespace MattMathTests
{
	constexpr float EPSILON_F = 0.000001f;
	
	TEST_SUITE("MattMathTests")
	{
		TEST_CASE("cross is the signed parallelogram area, and it is a scalar")
		{
			const Vector2F right(1.0f, 0.0f);
			const Vector2F down(0.0f, 1.0f);

			// The unit square, both ways round. The magnitude is the area and
			// the sign is the turn direction.
			CHECK(Vector2F::cross(right, down) == 1.0f);
			CHECK(Vector2F::cross(down, right) == -1.0f);

			// Scaling either side scales the area.
			CHECK(Vector2F::cross(right * 3.0f, down * 4.0f) == 12.0f);
		}
		TEST_CASE("cross is zero exactly when the two are parallel")
		{
			const Vector2F v(3.0f, 4.0f);

			CHECK(Vector2F::cross(v, v) == 0.0f);
			CHECK(Vector2F::cross(v, v * 5.0f) == 0.0f);

			// Anti-parallel is still parallel: it is the direction that has
			// collapsed, not the ordering.
			CHECK(Vector2F::cross(v, v * -2.0f) == 0.0f);

			// A zero vector spans no area with anything.
			CHECK(Vector2F::cross(v, Vector2F::ZERO) == 0.0f);
		}
		TEST_CASE("cross and dot answer opposite questions")
		{
			// dot vanishes at a right angle where cross is largest, and cross
			// vanishes when parallel where dot is largest. Together they are
			// the components of one rotation, so the squares sum to the
			// product of the squared lengths.
			const Vector2F a(3.0f, 4.0f);
			const Vector2F b(-2.0f, 7.0f);

			const float d = Vector2F::dot(a, b);
			const float c = Vector2F::cross(a, b);

			CHECK(d * d + c * c ==
				doctest::Approx(a.length_squared() * b.length_squared()));
		}
		TEST_CASE("cross on differences is ORIENT2D, matching signed_2D_tri_area")
		{
			// The identity the two headers claim about each other. If either
			// side is ever rewritten, this is what notices.
			const Point2F a(0.0f, 0.0f);
			const Point2F b(10.0f, 0.0f);
			const Point2F c(0.0f, 10.0f);

			CHECK(Vector2F::cross(b - a, c - a) == signed_2D_tri_area(a, b, c));
			CHECK(Vector2F::cross(c - a, b - a) == signed_2D_tri_area(a, c, b));

			// Collinear points, from both functions.
			const Point2F far_along(20.0f, 0.0f);
			CHECK(Vector2F::cross(b - a, far_along - a) == 0.0f);
			CHECK(signed_2D_tri_area(a, b, far_along) == 0.0f);
		}
		TEST_CASE("cross separates slivers that a truncating orientation test loses")
		{
			// The reason this function exists as a float. A triangle a tenth
			// of a unit tall is a real triangle; the deleted mattmath::sign
			// cast its area to int and reported every one of these collinear,
			// which is the failure that turns a thin ramp into a line.
			const Point2F a(0.0f, 0.0f);
			const Point2F b(1.0f, 0.0f);
			const Point2F barely_above(0.0f, 0.1f);

			const float area = Vector2F::cross(b - a, barely_above - a);

			CHECK(area == doctest::Approx(0.1f));
			CHECK(area != 0.0f);
			CHECK(static_cast<int>(area) == 0);
		}
		TEST_CASE("a Quad must be convex, not merely non-self-intersecting")
		{
			// A dart: the square with one corner pushed back through the
			// opposite diagonal. No two of its edges cross, so the old
			// simplicity test accepted it - and then the separating-axis
			// theorem, which only decides convex shapes, produced a confident
			// wrong manifold for it.
			CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
				Point2F(2.0f, 2.0f), Point2F(0.0f, 10.0f)),
				std::invalid_argument);

			// A bowtie, which the old test also caught, since its edges do
			// cross.
			CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
				Point2F(0.0f, 10.0f), Point2F(10.0f, 10.0f)),
				std::invalid_argument);

			// Degenerate: three points on a line, and a repeated vertex. Both
			// put a zero on one side of a diagonal, and a zero is not strictly
			// opposite anything.
			CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(5.0f, 0.0f),
				Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f)),
				std::invalid_argument);
			CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(0.0f, 0.0f),
				Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f)),
				std::invalid_argument);

			// Convex quads are still fine, wound either way.
			CHECK_NOTHROW(Quad(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
				Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f)));
			CHECK_NOTHROW(Quad(Point2F(0.0f, 10.0f), Point2F(10.0f, 10.0f),
				Point2F(10.0f, 0.0f), Point2F(0.0f, 0.0f)));
		}
		TEST_CASE("test_8_cardinal_direction")
		{
			Point2F p(0.0f, 0.0f);
			Point2F q(1.0f, 1.0f);
			Vector2F v = Vector2F::unit_vector(q - p);
			Vector2F cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_DOWN_RIGHT);
			q = Point2F(-1.0f, 1.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_DOWN_LEFT);
			q = Point2F(-1.0f, -1.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_UP_LEFT);
			q = Point2F(1.0f, -1.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_UP_RIGHT);
			q = Point2F(0.0f, 1.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_DOWN);
			q = Point2F(-1.0f, 0.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_LEFT);
			q = Point2F(0.0f, -1.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_UP);
			q = Point2F(1.0f, 0.0f);
			v = Vector2F::unit_vector(q - p);
			cardinal = Vector2F::direction_to_8_cardinal_direction(v);
			CHECK(cardinal == Vector2F::DIRECTION_RIGHT);
		}
		TEST_CASE("test_rectangles_intersect")
		{
			RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);

			RectangleF b(5.0f, 5.0f, 10.0f, 10.0f);
			CHECK(rectangles_intersect(a, b));

			// a and b are just touching
			b = RectangleF(10.0f - EPSILON_F, 10.0f, 10.0f, 10.0f);
			CHECK(rectangles_intersect(a, b));

			// a and b are just not touching
			b = RectangleF(10.0f + EPSILON_F, 10.0f, 10.0f, 10.0f);
			CHECK_FALSE(rectangles_intersect(a, b));

			// b is inside a
			b = RectangleF(2.0f, 2.0f, 2.0f, 2.0f);
			CHECK(rectangles_intersect(a, b));

			// a is inside b
			b = RectangleF(-2.0f, -2.0f, 20.0f, 20.0f);
			CHECK(rectangles_intersect(a, b));

			// 2 identical rectangles
			CHECK(rectangles_intersect(a, a));
		}
		TEST_CASE("test_rectangle_circle_intersect")
		{
			RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);
			Point2F p;

			Circle c(5.0f, 5.0f, 5.0f);
			CHECK(rectangle_circle_intersect(a, c, p));
			CHECK(p == Point2F(5.0f, 5.0f));

			c = Circle(15.0f, 15.0f, 5.0f);
			CHECK_FALSE(rectangle_circle_intersect(a, c, p));
			CHECK(p == Point2F(10.0f, 10.0f));

			// c is inside a
			c = Circle(2.0f, 2.0f, 2.0f);
			CHECK(rectangle_circle_intersect(a, c, p));
			CHECK(p == Point2F(2.0f, 2.0f));

			// a is inside c
			c = Circle(5.0f, 5.0f, 10.0f);
			CHECK(rectangle_circle_intersect(a, c, p));
			CHECK(p == Point2F(5.0f, 5.0f));

			// a and c are just touching
			c = Circle(15.0f - EPSILON_F, 5.0f, 5.0f);
			CHECK(rectangle_circle_intersect(a, c, p));
			CHECK(p == Point2F(10.0f, 5.0f));

			// a and c are just not touching
			c = Circle(15.0f + EPSILON_F, 5.0f, 5.0f);
			CHECK_FALSE(rectangle_circle_intersect(a, c, p));
			CHECK(p == Point2F(10.0f, 5.0f));
		}
		TEST_CASE("test_rectangle_triangle_intersect")
		{
			RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);

			Triangle t(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f), Point2F(5.0f, 15.0f));
			CHECK(rectangle_triangle_intersect(a, t));

			t = Triangle(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f), Point2F(1500.0f, 1500.0f));
			CHECK_FALSE(rectangle_triangle_intersect(a, t));

			// t is inside a
			t = Triangle(Point2F(2.0f, 2.0f), Point2F(2.0f, 8.0f), Point2F(8.0f, 2.0f));
			CHECK(rectangle_triangle_intersect(a, t));

			// a is inside t
			t = Triangle(Point2F(-2.0f, -2.0f), Point2F(12.0f, -2.0f), Point2F(-2.0f, 12.0f));
			CHECK(rectangle_triangle_intersect(a, t));

			// a and t are just touching
			t = Triangle(Point2F(10.0f - FLT_EPSILON, 10.0f), Point2F(20.0f, 10.0f),
				Point2F(10.0f - FLT_EPSILON, 20.0f));
			CHECK(rectangle_triangle_intersect(a, t));

			// a and t are just not touching
			t = Triangle(Point2F(10.0f + FLT_EPSILON, 10.0f), Point2F(20.0f, 10.0f),
				Point2F(10.0f + FLT_EPSILON, 20.0f));
		}
		TEST_CASE("test_rectangle_quad_intersect")
		{
			RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);

			Quad q(RectangleF(5.0f, 5.0f, 10.0f, 10.0f));
			CHECK(rectangle_quad_intersect(a, q));

			q = Quad(RectangleF(500.0f, 500.0f, 10.0f, 10.0f));
			CHECK_FALSE(rectangle_quad_intersect(a, q));

			// q is inside a
			q = Quad(RectangleF(2.0f, 2.0f, 2.0f, 2.0f));
			CHECK(rectangle_quad_intersect(a, q));

			// a is inside q
			q = Quad(RectangleF(-2.0f, -2.0f, 20.0f, 20.0f));
			CHECK(rectangle_quad_intersect(a, q));

			// a and q are just touching
			q = Quad(RectangleF(10.0f - EPSILON_F, 10.0f, 10.0f, 10.0f));
			CHECK(rectangle_quad_intersect(a, q));

			// a and q are just not touching
			q = Quad(RectangleF(10.0f + EPSILON_F, 10.0f, 10.0f, 10.0f));
			CHECK_FALSE(rectangle_quad_intersect(a, q));

			// 2 identical rectangles
			q = Quad(a);
			CHECK(rectangle_quad_intersect(a, q));
		}
		TEST_CASE("test_rectangle_segment_intersect")
		{
			RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);

			Segment s(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f));
			CHECK(rectangle_segment_intersect(a, s));

			s = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
			CHECK_FALSE(rectangle_segment_intersect(a, s));

			// s is inside a
			s = Segment(Point2F(2.0f, 2.0f), Point2F(8.0f, 8.0f));
			CHECK(rectangle_segment_intersect(a, s));

			// a is inside s
			s = Segment(Point2F(-2.0f, -2.0f), Point2F(12.0f, 12.0f));
			CHECK(rectangle_segment_intersect(a, s));

			// a and s are just touching
			s = Segment(Point2F(10.0f - EPSILON_F, 10.0f), Point2F(20.0f, 10.0f));
			CHECK(rectangle_segment_intersect(a, s));

			// a and s are just not touching
			s = Segment(Point2F(10.0f + EPSILON_F_2, 10.0f), Point2F(20.0f, 10.0f));
			CHECK_FALSE(rectangle_segment_intersect(a, s));
		}
		TEST_CASE("test_rectangle_point_intersect")
		{
			RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);

			Point2F p(5.0f, 5.0f);
			CHECK(rectangle_point_intersect(a, p));

			p = Point2F(15.0f, 15.0f);
			CHECK_FALSE(rectangle_point_intersect(a, p));

			// p is on the edge of a
			p = Point2F(10.0f, 10.0f);
			CHECK(rectangle_point_intersect(a, p));

			// p is on the corner of a
			p = Point2F(0.0f, 0.0f);
			CHECK(rectangle_point_intersect(a, p));
		}
		TEST_CASE("test_rectangle_rotated_rectangle_intersect")
		{
			// first test with axes aligned
			RectangleRotated rr = RectangleRotated(Vector2F::ZERO,
				Vector2F::DIRECTION_RIGHT,
				Vector2F::DIRECTION_UP, Vector2F(5.0f, 5.0f));

			RectangleF r(0.0f, 0.0f, 10.0f, 10.0f);
			CHECK(rectangle_rotated_rectangle_intersect(r, rr));

			r = RectangleF(5.0f - EPSILON_F, 5.0f - EPSILON_F, 10.0f, 10.0f);
			CHECK(rectangle_rotated_rectangle_intersect(r, rr));

			r = RectangleF(5.0f + EPSILON_F, 5.0f + EPSILON_F, 10.0f, 10.0f);
			CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));

			r = RectangleF(10.0f, 10.0f, 10.0f, 10.0f);
			CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));
			
			// second test with axes not aligned
			rr = RectangleRotated(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			r = RectangleF(0.0f, 0.0f, 10.0f, 10.0f);
			CHECK(rectangle_rotated_rectangle_intersect(r, rr));

			// rectangle is inside rotated rectangle
			r = RectangleF(2.0f, 2.0f, 2.0f, 2.0f);
			CHECK(rectangle_rotated_rectangle_intersect(r, rr));

			// rotated rectangle is inside rectangle
			r = RectangleF(-2.0f, -2.0f, 20.0f, 20.0f);
			CHECK(rectangle_rotated_rectangle_intersect(r, rr));

			Vector2F v = Vector2F::DIRECTION_DOWN_RIGHT * 10.0f;
			Vector2F v_touching = v - Vector2F::DIRECTION_DOWN_RIGHT * EPSILON_F;
			Vector2F v_not_touching = v + Vector2F::DIRECTION_DOWN_RIGHT * EPSILON_F_100;

			r = RectangleF(v_touching.x, v_touching.y, 10.0f, 10.0f);
			CHECK(rectangle_rotated_rectangle_intersect(r, rr));

			r = RectangleF(v_not_touching.x, v_not_touching.y, 10.0f, 10.0f);
			CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));

			r = RectangleF(15.0f, 15.0f, 10.0f, 10.0f);
			CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));
		}
		TEST_CASE("test_circles_intersect")
		{
			Circle a(0.0f, 0.0f, 10.0f);

			Circle b(5.0f, 5.0f, 5.0f);
			CHECK(circles_intersect(a, b));

			// a and b are just touching
			b = Circle(10.0f - EPSILON_F, 5.0f, 5.0f);
			CHECK(circles_intersect(a, b));

			// a and b are just not touching
			b = Circle(15.0f + EPSILON_F, 0.0f, 5.0f);
			CHECK_FALSE(circles_intersect(a, b));

			// b is inside a
			b = Circle(2.0f, 2.0f, 2.0f);
			CHECK(circles_intersect(a, b));

			// a is inside b
			b = Circle(0.0f, 0.0f, 20.0f);
			CHECK(circles_intersect(a, b));

			// 2 identical circles
			CHECK(circles_intersect(a, a));
		}
		TEST_CASE("test_circle_triangle_intersect")
		{
			Circle a(0.0f, 0.0f, 10.0f);
			Point2F p;

			Triangle t(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f), Point2F(5.0f, 15.0f));
			CHECK(circle_triangle_intersect(a, t, p));
			CHECK(p == Point2F(5.0f, 5.0f));

			t = Triangle(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f), Point2F(1500.0f, 1500.0f));
			CHECK_FALSE(circle_triangle_intersect(a, t, p));
			CHECK(p == Point2F(500.0f, 500.0f));

			// t is inside a
			t = Triangle(Point2F(2.0f, 2.0f), Point2F(2.0f, 8.0f), Point2F(8.0f, 2.0f));
			CHECK(circle_triangle_intersect(a, t, p));
			CHECK(p == Point2F(2.0f, 2.0f));

			// a is inside t
			t = Triangle(Point2F(-2.0f, -2.0f), Point2F(12.0f, -2.0f), Point2F(-2.0f, 12.0f));
			CHECK(circle_triangle_intersect(a, t, p));
			CHECK(p == Point2F(0.0f, 0.0f));

			// a and t are just touching
			t = Triangle(Point2F(10.0f - EPSILON_F, 0.0f), Point2F(20.0f, 0.0f),
				Point2F(10.0f - EPSILON_F, 20.0f));
			CHECK(circle_triangle_intersect(a, t, p));
			CHECK(are_equal(p, Point2F(10.0f - EPSILON_F, 0.0f), 0.1f));

			// a and t are just not touching
			t = Triangle(Point2F(10.0f + EPSILON_F, 10.0f), Point2F(20.0f, 10.0f),
				Point2F(10.0f + EPSILON_F, 20.0f));
			CHECK_FALSE(circle_triangle_intersect(a, t, p));
			CHECK(p == Point2F(10.0f + EPSILON_F, 10.0f));
		}
		TEST_CASE("test_circle_quad_intersect")
		{
			Circle a(0.0f, 0.0f, 10.0f);

			Quad q(RectangleF(5.0f, 5.0f, 10.0f, 10.0f));
			CHECK(circle_quad_intersect(a, q));

			q = Quad(RectangleF(500.0f, 500.0f, 10.0f, 10.0f));
			CHECK_FALSE(circle_quad_intersect(a, q));

			// q is inside a
			q = Quad(RectangleF(2.0f, 2.0f, 2.0f, 2.0f));
			CHECK(circle_quad_intersect(a, q));

			// a is inside q
			q = Quad(RectangleF(-2.0f, -2.0f, 20.0f, 20.0f));
			CHECK(circle_quad_intersect(a, q));

			// a and q are just touching
			q = Quad(RectangleF(10.0f - EPSILON_F, 0.0f, 10.0f, 10.0f));
			CHECK(circle_quad_intersect(a, q));

			// a and q are just not touching
			q = Quad(RectangleF(10.0f + EPSILON_F, 10.0f, 10.0f, 10.0f));
			CHECK_FALSE(circle_quad_intersect(a, q));
		}
		TEST_CASE("test_circle_rectangle_rotated_intersect")
		{
			RectangleRotated rr(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			Circle c(5.0f, 5.0f, 10.0f);
			CHECK(circle_rectangle_rotated_intersect(c, rr));

			// c is inside rr
			c = Circle(0.0f, 0.0f, 2.0f);
			CHECK(circle_rectangle_rotated_intersect(c, rr));

			// rr is inside c
			c = Circle(0.0f, 0.0f, 200.0f);
			CHECK(circle_rectangle_rotated_intersect(c, rr));

			// c and rr are just touching
			c = Circle(Vector2F::DIRECTION_DOWN_RIGHT * (20.0f - EPSILON_F), 10.0f);
			CHECK(circle_rectangle_rotated_intersect(c, rr));

			// c and rr are just not touching
			c = Circle(Vector2F::DIRECTION_DOWN_RIGHT * (20.0f + EPSILON_F), 10.0f);
			CHECK_FALSE(circle_rectangle_rotated_intersect(c, rr));
		}
		TEST_CASE("test_circle_segment_intersect")
		{
			Circle c(0.0f, 0.0f, 10.0f);
			Point2F p;

			Segment s(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f));
			CHECK(circle_segment_intersect(c, s, p));
			CHECK(p == Point2F(5.0f, 5.0f));

			s = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
			CHECK_FALSE(circle_segment_intersect(c, s, p));
			CHECK(p == Point2F(500.0f, 500.0f));

			// s is inside c
			s = Segment(Point2F(2.0f, 2.0f), Point2F(8.0f, 8.0f));
			CHECK(circle_segment_intersect(c, s, p));
			CHECK(p == Point2F(2.0f, 2.0f));

			// c is inside s
			s = Segment(Point2F(-2.0f, -2.0f), Point2F(12.0f, 12.0f));
			CHECK(circle_segment_intersect(c, s, p));
			CHECK(p == Point2F(0.0f, 0.0f));

			// c and s are just touching
			s = Segment(Point2F(10.0f - EPSILON_F, 0.0f), Point2F(20.0f, 0.0f));
			CHECK(circle_segment_intersect(c, s, p));
			CHECK(are_equal(p, Point2F(10.0f - EPSILON_F, 0.0f), 0.1f));

			// c and s are just not touching
			s = Segment(Point2F(10.0f + EPSILON_F, 0.0f), Point2F(20.0f, 0.0f));
			CHECK_FALSE(circle_segment_intersect(c, s, p));
			CHECK(p == Point2F(10.0f + EPSILON_F, 0.0f));
		}
		TEST_CASE("test_triangles_intersect")
		{
			Triangle a(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f));

			Triangle b(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f), Point2F(5.0f, 15.0f));
			CHECK(triangles_intersect(a, b));

			b = Triangle(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f), Point2F(1500.0f, 1500.0f));
			CHECK_FALSE(triangles_intersect(a, b));

			// b is inside a
			b = Triangle(Point2F(2.0f, 2.0f), Point2F(2.0f, 8.0f), Point2F(8.0f, 2.0f));
			CHECK(triangles_intersect(a, b));

			// a is inside b
			b = Triangle(Point2F(-2.0f, -2.0f), Point2F(12.0f, -2.0f), Point2F(-2.0f, 12.0f));
			CHECK(triangles_intersect(a, b));

			// a and b are just touching
			b = Triangle(Point2F(10.0f - EPSILON_F, 0.0f), Point2F(20.0f, 0.0f),
				Point2F(10.0f - EPSILON_F, 10.0f));
			CHECK(triangles_intersect(a, b));

			// a and b are just not touching
			b = Triangle(Point2F(10.0f + EPSILON_F, 00.0f), Point2F(20.0f, 00.0f),
				Point2F(10.0f + EPSILON_F, 10.0f));
			CHECK_FALSE(triangles_intersect(a, b));
		}
		TEST_CASE("test_triangle_quad_intersect")
		{
			Triangle a(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f));

			Quad q(RectangleF(5.0f, 5.0f, 10.0f, 10.0f));
			CHECK(triangle_quad_intersect(a, q));

			q = Quad(RectangleF(500.0f, 500.0f, 10.0f, 10.0f));
			CHECK_FALSE(triangle_quad_intersect(a, q));

			// q is inside a
			q = Quad(RectangleF(2.0f, 2.0f, 2.0f, 2.0f));
			CHECK(triangle_quad_intersect(a, q));

			// a is inside q
			q = Quad(RectangleF(-2.0f, -2.0f, 20.0f, 20.0f));
			CHECK(triangle_quad_intersect(a, q));

			// a and q are just touching
			q = Quad(RectangleF(10.0f - EPSILON_F, 0.0f, 10.0f, 10.0f));
			CHECK(triangle_quad_intersect(a, q));

			// a and q are just not touching
			q = Quad(RectangleF(10.0f + EPSILON_F, 0.0f, 10.0f, 10.0f));
			CHECK_FALSE(triangle_quad_intersect(a, q));
		}
		TEST_CASE("test_triangle_segment_intersect")
		{
			Triangle a(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f));

			Segment s(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f));
			CHECK(triangle_segment_intersect(a, s));

			s = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
			CHECK_FALSE(triangle_segment_intersect(a, s));

			// s is inside a
			s = Segment(Point2F(2.0f, 2.0f), Point2F(8.0f, 8.0f));
			CHECK(triangle_segment_intersect(a, s));

			// a is inside s
			s = Segment(Point2F(-2.0f, -2.0f), Point2F(12.0f, 12.0f));
			CHECK(triangle_segment_intersect(a, s));

			// a and s are just touching
			s = Segment(Point2F(10.0f - EPSILON_F, 0.0f), Point2F(20.0f, 0.0f));
			CHECK(triangle_segment_intersect(a, s));

			// a and s are just not touching
			s = Segment(Point2F(10.0f + EPSILON_F, 0.0f), Point2F(20.0f, 0.0f));
			CHECK_FALSE(triangle_segment_intersect(a, s));
		}
		TEST_CASE("test_triangle_rectangle_rotated_intersect")
		{
			RectangleRotated rr(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			Triangle t(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f));
			CHECK(triangle_rectangle_rotated_intersect(t, rr));

			// t is inside rr
			t = Triangle(Point2F(2.0f, 2.0f), Point2F(2.0f, 3.0f), Point2F(3.0f, 2.0f));
			CHECK(triangle_rectangle_rotated_intersect(t, rr));

			// rr is inside t
			t = Triangle(Point2F(-200.0f, -200.0f), Point2F(1000.0f, -200.0f),
				Point2F(-200.0f, 1000.0f));
			CHECK(triangle_rectangle_rotated_intersect(t, rr));

			t = Triangle(Point2F(-500.0f, -500.0f), Point2F(-500.0f, -400.0f),
				Point2F(-400.0f, -500.0f));
			CHECK_FALSE(triangle_rectangle_rotated_intersect(t, rr));

			// t and rr are just touching
			t = Triangle(Vector2F::DIRECTION_DOWN_RIGHT * (10.0f - EPSILON_F),
				Vector2F::DIRECTION_DOWN_RIGHT * 20.0f,
				Vector2F::DIRECTION_DOWN_RIGHT * 20.0f + Vector2F::DIRECTION_UP_RIGHT * 10.0f);
			CHECK(triangle_rectangle_rotated_intersect(t, rr));

			// t and rr are just not touching
			t = Triangle(Vector2F::DIRECTION_DOWN_RIGHT * (10.0f + EPSILON_F_100),
				Vector2F::DIRECTION_DOWN_RIGHT * 20.0f,
				Vector2F::DIRECTION_DOWN_RIGHT * 20.0f + Vector2F::DIRECTION_UP_RIGHT * 10.0f);
			CHECK_FALSE(triangle_rectangle_rotated_intersect(t, rr));
		}
		TEST_CASE("test_quads_intersect")
		{
			Quad a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f));

			Quad b(RectangleF(5.0f, 5.0f, 10.0f, 10.0f));
			CHECK(quads_intersect(a, b));

			b = Quad(RectangleF(500.0f, 500.0f, 10.0f, 10.0f));
			CHECK_FALSE(quads_intersect(a, b));

			// b is inside a
			b = Quad(RectangleF(2.0f, 2.0f, 2.0f, 2.0f));
			CHECK(quads_intersect(a, b));

			// a is inside b
			b = Quad(RectangleF(-2.0f, -2.0f, 20.0f, 20.0f));
			CHECK(quads_intersect(a, b));

			// a and b are just touching
			b = Quad(RectangleF(10.0f - EPSILON_F, 0.0f, 10.0f, 10.0f));
			CHECK(quads_intersect(a, b));

			// a and b are just not touching
			b = Quad(RectangleF(10.0f + EPSILON_F_2, 0.0f, 10.0f, 10.0f));
			CHECK_FALSE(quads_intersect(a, b));

			// 2 identical quads
			CHECK(quads_intersect(a, a));
		}
		TEST_CASE("test_quad_segment_intersect")
		{
			Quad a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f));

			Segment s(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f));
			CHECK(quad_segment_intersect(a, s));

			s = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
			CHECK_FALSE(quad_segment_intersect(a, s));

			// s is inside a
			s = Segment(Point2F(2.0f, 2.0f), Point2F(8.0f, 8.0f));
			CHECK(quad_segment_intersect(a, s));

			// a is inside s
			s = Segment(Point2F(-5.0f, 5.0f), Point2F(15.0f, 5.0f));
			CHECK(quad_segment_intersect(a, s));

			// a and s are just touching
			s = Segment(Point2F(10.0f - EPSILON_F, 0.0f), Point2F(20.0f, 0.0f));
			CHECK(quad_segment_intersect(a, s));

			// a and s are just not touching
			s = Segment(Point2F(10.0f + EPSILON_F, 0.0f), Point2F(20.0f, 0.0f));
			CHECK_FALSE(quad_segment_intersect(a, s));
		}
		TEST_CASE("test_quad_point_intersect")
		{
			Quad a(RectangleF(0.0f, 0.0f, 10.0f, 10.0f));

			Point2F p(5.0f, 5.0f);
			CHECK(quad_point_intersect(a, p));

			p = Point2F(15.0f, 15.0f);
			CHECK_FALSE(quad_point_intersect(a, p));

			// p is on the edge of a
			p = Point2F(10.0f, 10.0f);
			CHECK(quad_point_intersect(a, p));

			// p is on the corner of a
			p = Point2F(0.0f, 0.0f);
			CHECK(quad_point_intersect(a, p));
		}
		TEST_CASE("test_quad_rectangle_rotated_intersect")
		{
			RectangleRotated rr(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			Quad q(RectangleF(5.0f, 5.0f, 10.0f, 10.0f));
			CHECK(quad_rectangle_rotated_intersect(q, rr));

			q = Quad(RectangleF(500.0f, 500.0f, 10.0f, 10.0f));
			CHECK_FALSE(quad_rectangle_rotated_intersect(q, rr));

			// q is inside rr
			q = Quad(RectangleF(2.0f, 2.0f, 2.0f, 2.0f));
			CHECK(quad_rectangle_rotated_intersect(q, rr));

			// rr is inside q
			q = Quad(RectangleF(-200.0f, -200.0f, 1000.0f, 1000.0f));
			CHECK(quad_rectangle_rotated_intersect(q, rr));

			// q and rr are just touching
			q = Quad(RectangleF(Vector2F::DIRECTION_DOWN_RIGHT * (10.0f - EPSILON_F),
				Vector2F(10.0f, 10.0f)));
			CHECK(quad_rectangle_rotated_intersect(q, rr));

			// q and rr are just not touching
			q = Quad(RectangleF(Vector2F::DIRECTION_DOWN_RIGHT * (10.0f + EPSILON_F_100),
				Vector2F(10.0f, 10.0f)));
			CHECK_FALSE(quad_rectangle_rotated_intersect(q, rr));
		}
		TEST_CASE("test_segments_intersect")
		{
			Segment a(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));
			Segment b(Point2F(5.0f, -5.0f), Point2F(5.0f, 10.0f));
			CHECK(segments_intersect(a, b));
			b = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
			CHECK_FALSE(segments_intersect(a, b));
			// a and b are just touching
			b = Segment(Point2F(5.0f, -5.0f), Point2F(5.0f, EPSILON_F));
			CHECK(segments_intersect(a, b));
			// a and b are just not touching
			b = Segment(Point2F(5.0f, -5.0f), Point2F(5.0f, -EPSILON_F));
			CHECK_FALSE(segments_intersect(a, b));
			// a and b are equal
			b = Segment(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));
			CHECK(segments_intersect(a, b));
		}
		TEST_CASE("test_segment_rectangle_rotated_intersect")
		{
			 RectangleRotated rr(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			 Segment s(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f));
			 CHECK(segment_rectangle_rotated_intersect(s, rr));

			 s = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
			 CHECK_FALSE(segment_rectangle_rotated_intersect(s, rr));

			 // s is inside rr
			 s = Segment(Point2F(2.0f, 2.0f), Point2F(3.0f, 3.0f));
			 CHECK(segment_rectangle_rotated_intersect(s, rr));
		}
		TEST_CASE("test_point_rectangle_rotated_intersect")
		{
			RectangleRotated rr(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			Point2F p(5.0f, 5.0f);
			CHECK(point_rectangle_rotated_intersect(p, rr));

			p = Point2F(15.0f, 15.0f);
			CHECK_FALSE(point_rectangle_rotated_intersect(p, rr));

			// p is on the edge of rr
			p = Vector2F::DIRECTION_DOWN_RIGHT * 10.0f;
			CHECK(point_rectangle_rotated_intersect(p, rr));

			// p is just outside the edge of rr
			p = Vector2F::DIRECTION_DOWN_RIGHT * (10.0f + EPSILON_F_100);
			CHECK_FALSE(point_rectangle_rotated_intersect(p, rr));

			// p is on the corner of rr
			p = Vector2F::DIRECTION_DOWN_RIGHT * 10.0f + Vector2F::DIRECTION_UP_RIGHT * 10.0f;
			CHECK(point_rectangle_rotated_intersect(p, rr));

			// p is just outside the corner of rr
			p = Vector2F::DIRECTION_DOWN_RIGHT * (10.0f + EPSILON_F_100) +
				Vector2F::DIRECTION_UP_RIGHT * (10.0f + EPSILON_F_100);

			// p is inside rr
			p = Point2F(2.0f, 2.0f);
			CHECK(point_rectangle_rotated_intersect(p, rr));
		}
		TEST_CASE("test_rectangles_rotated_intersect")
		{
			RectangleRotated a(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));

			RectangleRotated b(Vector2F::DIRECTION_DOWN_RIGHT * 5.0f,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));
			CHECK(rectangles_rotated_intersect(a, b));

			b = RectangleRotated(Vector2F::DIRECTION_DOWN_RIGHT * 200.0f,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));
			CHECK_FALSE(rectangles_rotated_intersect(a, b));

			// b is inside a
			b = RectangleRotated(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(2.0f, 2.0f));
			CHECK(rectangles_rotated_intersect(a, b));

			// a is inside b
			b = RectangleRotated(Vector2F::ZERO,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(40.0f, 40.0f));
			CHECK(rectangles_rotated_intersect(a, b));

			// a and b are just touching
			b = RectangleRotated(Vector2F::DIRECTION_DOWN_RIGHT * 20.0f,
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));
			CHECK(rectangles_rotated_intersect(a, b));

			// a and b are just not touching
			b = RectangleRotated(Vector2F::DIRECTION_DOWN_RIGHT * (20.0f + EPSILON_F_100),
				Vector2F::DIRECTION_UP_RIGHT,
				Vector2F::DIRECTION_DOWN_RIGHT, Vector2F(10.0f, 10.0f));
			CHECK_FALSE(rectangles_rotated_intersect(a, b));

			// 2 identical rectangles
			CHECK(rectangles_rotated_intersect(a, a));
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
			CHECK(are_equal(rr.center().x, 5.0f, EPSILON_F_100));
			CHECK(are_equal(rr.center().y, 0.0f, EPSILON_F_100));

			// x axis runs along the segment and is a unit vector
			CHECK(are_equal(rr.x_axis().length(), 1.0f, EPSILON_F_100));
			CHECK(are_equal(rr.y_axis().length(), 1.0f, EPSILON_F_100));
			CHECK(are_equal(rr.x_axis().x, 1.0f, EPSILON_F_100));
			CHECK(are_equal(rr.x_axis().y, 0.0f, EPSILON_F_100));

			// half extents: half the length plus the thickness along the
			// segment, the thickness across it
			CHECK(are_equal(rr.half_x_width(),
				5.0f + thickness, EPSILON_F_100));
			CHECK(are_equal(rr.half_y_width(),
				thickness, EPSILON_F_100));

			// a diagonal segment stays valid and picks up the right angle
			Segment diagonal(Point2F(0.0f, 0.0f), Point2F(10.0f, 10.0f));
			RectangleRotated rr_diagonal(diagonal, 1.0f);
			CHECK(rr_diagonal.is_valid());
			CHECK(are_equal(rr_diagonal.x_axis().length(),
				1.0f, EPSILON_F_100));
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
			CHECK(are_equal(rr.half_x_width(), 5.0f, EPSILON_F_100));
			CHECK(rr.is_valid());

			CHECK_THROWS_AS([&] { rr.set_half_extents(Vector2F(0.0f, 0.0f)); }(), std::invalid_argument);
			CHECK(are_equal(rr.half_x_width(), 5.0f, EPSILON_F_100));
			CHECK(are_equal(rr.half_y_width(), 5.0f, EPSILON_F_100));

			CHECK_THROWS_AS([&] { rr.set_x_axis(Vector2F::DIRECTION_DOWN_RIGHT); }(), std::invalid_argument);
			CHECK(are_equal(rr.x_axis().x, 1.0f, EPSILON_F_100));
			CHECK(rr.is_valid());

			CHECK_THROWS_AS([&] { rr.inflate(-10.0f); }(), std::invalid_argument);
			CHECK(are_equal(rr.half_x_width(), 5.0f, EPSILON_F_100));
			CHECK(rr.is_valid());

			// a valid inflate still works, and keeps the corner cache in sync
			rr.inflate(1.0f);
			CHECK(are_equal(rr.half_x_width(), 6.0f, EPSILON_F_100));
			CHECK(are_equal(rr.point_2().x, 6.0f, EPSILON_F_100));
			CHECK(rr.is_valid());
		}
	};

} // namespace MattMathTests
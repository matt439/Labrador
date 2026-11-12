#include <doctest/doctest.h>

#include "engine/math/circle.h"
#include "engine/math/intersects.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/scalar.h"
#include "engine/math/segment.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

#include <cfloat>
#include <limits>

using namespace labrador;

constexpr float epsilon_f = 0.000001f;
constexpr float epsilon_f_2 = 0.000002f;
constexpr float epsilon_f_100 = 0.0001f;

TEST_SUITE("ShapeIntersect")
{
	TEST_CASE("triangles overlapping with no vertex inside either are still found")
	{
		// The Star of David: two triangles crossing in a hexagon, with
		// every vertex of each outside the other, so the containment half
		// of the predicate finds nothing and the answer rests entirely on
		// edge crossings.
		//
		// This is the case the four-pair enumeration looked unable to
		// handle. It handled it - six crossings cannot all avoid four of
		// nine pairs - and no configuration defeats it, for the reason
		// argued in intersects.cpp. The test is here to pin the behaviour,
		// not to record a fix.
		const Triangle up(Point2F(0.0f, 0.0f), Point2F(60.0f, 0.0f),
			Point2F(30.0f, 52.0f));
		const Triangle down(Point2F(0.0f, 35.0f), Point2F(60.0f, 35.0f),
			Point2F(30.0f, -17.0f));

		for (int i = 0; i < 3; i++)
		{
			CAPTURE(i);
			REQUIRE_FALSE(triangle_point_intersect(up, down.points[i]));
			REQUIRE_FALSE(triangle_point_intersect(down, up.points[i]));
		}

		CHECK(triangles_intersect(up, down));
		CHECK(triangles_intersect(down, up));

		// And the predicate is symmetric on a plain miss.
		const Triangle far_away(Point2F(500.0f, 500.0f),
			Point2F(560.0f, 500.0f), Point2F(530.0f, 552.0f));
		CHECK_FALSE(triangles_intersect(up, far_away));
		CHECK_FALSE(triangles_intersect(far_away, up));
	}
	TEST_CASE("RectangleF::intersects, the one predicate kept as a member")
	{
		RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);

		RectangleF b(5.0f, 5.0f, 10.0f, 10.0f);
		CHECK(a.intersects(b));

		// a and b are just touching
		b = RectangleF(10.0f - epsilon_f, 10.0f, 10.0f, 10.0f);
		CHECK(a.intersects(b));

		// a and b are just not touching
		b = RectangleF(10.0f + epsilon_f, 10.0f, 10.0f, 10.0f);
		CHECK_FALSE(a.intersects(b));

		// b is inside a
		b = RectangleF(2.0f, 2.0f, 2.0f, 2.0f);
		CHECK(a.intersects(b));

		// a is inside b
		b = RectangleF(-2.0f, -2.0f, 20.0f, 20.0f);
		CHECK(a.intersects(b));

		// 2 identical rectangles
		CHECK(a.intersects(a));
	}
	TEST_CASE("a rectangle and a circle intersect when they overlap, nest or touch")
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
		c = Circle(15.0f - epsilon_f, 5.0f, 5.0f);
		CHECK(rectangle_circle_intersect(a, c, p));
		CHECK(p == Point2F(10.0f, 5.0f));

		// a and c are just not touching
		c = Circle(15.0f + epsilon_f, 5.0f, 5.0f);
		CHECK_FALSE(rectangle_circle_intersect(a, c, p));
		CHECK(p == Point2F(10.0f, 5.0f));
	}
	TEST_CASE("a rectangle and a triangle intersect when they overlap, nest or touch")
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

		// a and t are just not touching. FLT_EPSILON is no gap at 10: the
		// spacing between floats there is eight times larger, so
		// 10.0f + FLT_EPSILON rounds back to 10.0f and touches a's corner.
		t = Triangle(Point2F(10.0f + epsilon_f_100, 10.0f), Point2F(20.0f, 10.0f),
			Point2F(10.0f + epsilon_f_100, 20.0f));
		CHECK_FALSE(rectangle_triangle_intersect(a, t));
	}
	TEST_CASE("a rectangle and a quad intersect when they overlap, nest or touch")
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
		q = Quad(RectangleF(10.0f - epsilon_f, 10.0f, 10.0f, 10.0f));
		CHECK(rectangle_quad_intersect(a, q));

		// a and q are just not touching
		q = Quad(RectangleF(10.0f + epsilon_f, 10.0f, 10.0f, 10.0f));
		CHECK_FALSE(rectangle_quad_intersect(a, q));

		// 2 identical rectangles
		q = Quad(a);
		CHECK(rectangle_quad_intersect(a, q));
	}
	TEST_CASE("a rectangle and a segment intersect when they cross, nest or touch")
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
		s = Segment(Point2F(10.0f - epsilon_f, 10.0f), Point2F(20.0f, 10.0f));
		CHECK(rectangle_segment_intersect(a, s));

		// a and s are just not touching
		s = Segment(Point2F(10.0f + epsilon_f_2, 10.0f), Point2F(20.0f, 10.0f));
		CHECK_FALSE(rectangle_segment_intersect(a, s));
	}
	TEST_CASE("a point inside a rectangle or on its boundary intersects it")
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
	TEST_CASE("a rectangle and a rotated rectangle intersect, aligned or not, and when nested")
	{
		// first test with axes aligned
		RectangleRotated rr = RectangleRotated(Vector2F::zero,
			Vector2F::direction_right,
			Vector2F::direction_up, Vector2F(5.0f, 5.0f));

		RectangleF r(0.0f, 0.0f, 10.0f, 10.0f);
		CHECK(rectangle_rotated_rectangle_intersect(r, rr));

		r = RectangleF(5.0f - epsilon_f, 5.0f - epsilon_f, 10.0f, 10.0f);
		CHECK(rectangle_rotated_rectangle_intersect(r, rr));

		r = RectangleF(5.0f + epsilon_f, 5.0f + epsilon_f, 10.0f, 10.0f);
		CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));

		r = RectangleF(10.0f, 10.0f, 10.0f, 10.0f);
		CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));

		// second test with axes not aligned
		rr = RectangleRotated(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

		r = RectangleF(0.0f, 0.0f, 10.0f, 10.0f);
		CHECK(rectangle_rotated_rectangle_intersect(r, rr));

		// rectangle is inside rotated rectangle
		r = RectangleF(2.0f, 2.0f, 2.0f, 2.0f);
		CHECK(rectangle_rotated_rectangle_intersect(r, rr));

		// rotated rectangle is inside rectangle
		r = RectangleF(-2.0f, -2.0f, 20.0f, 20.0f);
		CHECK(rectangle_rotated_rectangle_intersect(r, rr));

		Vector2F v = Vector2F::direction_down_right * 10.0f;
		Vector2F v_touching = v - Vector2F::direction_down_right * epsilon_f;
		Vector2F v_not_touching = v + Vector2F::direction_down_right * epsilon_f_100;

		r = RectangleF(v_touching.x, v_touching.y, 10.0f, 10.0f);
		CHECK(rectangle_rotated_rectangle_intersect(r, rr));

		r = RectangleF(v_not_touching.x, v_not_touching.y, 10.0f, 10.0f);
		CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));

		r = RectangleF(15.0f, 15.0f, 10.0f, 10.0f);
		CHECK_FALSE(rectangle_rotated_rectangle_intersect(r, rr));
	}
	TEST_CASE("two circles intersect when they overlap, nest or touch")
	{
		Circle a(0.0f, 0.0f, 10.0f);

		Circle b(5.0f, 5.0f, 5.0f);
		CHECK(circles_intersect(a, b));

		// a and b are just touching
		b = Circle(15.0f - epsilon_f, 0.0f, 5.0f);
		CHECK(circles_intersect(a, b));

		// a and b are just not touching
		b = Circle(15.0f + epsilon_f, 0.0f, 5.0f);
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
	TEST_CASE("a circle and a triangle intersect when they overlap, nest or touch, and report the closest point")
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
		t = Triangle(Point2F(10.0f - epsilon_f, 0.0f), Point2F(20.0f, 0.0f),
			Point2F(10.0f - epsilon_f, 20.0f));
		CHECK(circle_triangle_intersect(a, t, p));
		CHECK(are_equal(p, Point2F(10.0f - epsilon_f, 0.0f), 0.1f));

		// a and t are just not touching
		t = Triangle(Point2F(10.0f + epsilon_f, 0.0f), Point2F(20.0f, 0.0f),
			Point2F(10.0f + epsilon_f, 20.0f));
		CHECK_FALSE(circle_triangle_intersect(a, t, p));
		CHECK(p == Point2F(10.0f + epsilon_f, 0.0f));
	}
	TEST_CASE("a circle and a quad intersect when they overlap, nest or touch")
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
		q = Quad(RectangleF(10.0f - epsilon_f, 0.0f, 10.0f, 10.0f));
		CHECK(circle_quad_intersect(a, q));

		// a and q are just not touching
		q = Quad(RectangleF(10.0f + epsilon_f, 0.0f, 10.0f, 10.0f));
		CHECK_FALSE(circle_quad_intersect(a, q));
	}
	TEST_CASE("a circle and a rotated rectangle intersect when they overlap, nest or touch")
	{
		RectangleRotated rr(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

		Circle c(5.0f, 5.0f, 10.0f);
		CHECK(circle_rectangle_rotated_intersect(c, rr));

		// c is inside rr
		c = Circle(0.0f, 0.0f, 2.0f);
		CHECK(circle_rectangle_rotated_intersect(c, rr));

		// rr is inside c
		c = Circle(0.0f, 0.0f, 200.0f);
		CHECK(circle_rectangle_rotated_intersect(c, rr));

		// c and rr are just touching
		c = Circle(Vector2F::direction_down_right * (20.0f - epsilon_f), 10.0f);
		CHECK(circle_rectangle_rotated_intersect(c, rr));

		// c and rr are just not touching
		c = Circle(Vector2F::direction_down_right * (20.0f + epsilon_f), 10.0f);
		CHECK_FALSE(circle_rectangle_rotated_intersect(c, rr));
	}
	TEST_CASE("a circle and a segment intersect when they cross, nest or touch")
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
		s = Segment(Point2F(10.0f - epsilon_f, 0.0f), Point2F(20.0f, 0.0f));
		CHECK(circle_segment_intersect(c, s, p));
		CHECK(are_equal(p, Point2F(10.0f - epsilon_f, 0.0f), 0.1f));

		// c and s are just not touching
		s = Segment(Point2F(10.0f + epsilon_f, 0.0f), Point2F(20.0f, 0.0f));
		CHECK_FALSE(circle_segment_intersect(c, s, p));
		CHECK(p == Point2F(10.0f + epsilon_f, 0.0f));
	}
	TEST_CASE("a circle contains a point inside or on its boundary")
	{
		const Circle c(0.0f, 0.0f, 10.0f);

		CHECK(circle_point_intersect(c, Point2F(0.0f, 0.0f)));
		CHECK(circle_point_intersect(c, Point2F(3.0f, -4.0f)));

		// The boundary is inside: 10 squared against 10 squared, exactly.
		CHECK(circle_point_intersect(c, Point2F(10.0f, 0.0f)));
		CHECK(circle_point_intersect(c, Point2F(0.0f, -10.0f)));

		CHECK_FALSE(circle_point_intersect(c, Point2F(10.5f, 0.0f)));
		CHECK_FALSE(circle_point_intersect(c, Point2F(8.0f, 8.0f)));

		// A zero radius is a point, and contains itself and nothing else.
		const Circle dot(2.0f, 3.0f, 0.0f);
		CHECK(circle_point_intersect(dot, Point2F(2.0f, 3.0f)));
		CHECK_FALSE(circle_point_intersect(dot, Point2F(2.0f, 3.5f)));

		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float infinity = std::numeric_limits<float>::infinity();
		CHECK_FALSE(circle_point_intersect(c, Point2F(nan, 0.0f)));
		CHECK_FALSE(circle_point_intersect(c, Point2F(0.0f, nan)));
		CHECK_FALSE(circle_point_intersect(c, Point2F(infinity, 0.0f)));
		CHECK_FALSE(circle_point_intersect(Circle(nan, 0.0f, 10.0f), Point2F(0.0f, 0.0f)));
		CHECK_FALSE(circle_point_intersect(Circle(0.0f, 0.0f, nan), Point2F(0.0f, 0.0f)));
	}
	TEST_CASE("a circle is valid when its centre and radius are finite and the radius is not negative")
	{
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float infinity = std::numeric_limits<float>::infinity();

		CHECK(Circle(1.0f, 2.0f, 3.0f).is_valid());
		CHECK(Circle(1.0f, 2.0f, 0.0f).is_valid());
		CHECK(Circle().is_valid());

		CHECK_FALSE(Circle(1.0f, 2.0f, -3.0f).is_valid());
		CHECK_FALSE(Circle(1.0f, 2.0f, -0.0001f).is_valid());
		CHECK_FALSE(Circle(nan, 2.0f, 3.0f).is_valid());
		CHECK_FALSE(Circle(1.0f, nan, 3.0f).is_valid());
		CHECK_FALSE(Circle(1.0f, 2.0f, nan).is_valid());
		CHECK_FALSE(Circle(infinity, 2.0f, 3.0f).is_valid());
		CHECK_FALSE(Circle(1.0f, -infinity, 3.0f).is_valid());
		CHECK_FALSE(Circle(1.0f, 2.0f, infinity).is_valid());

		// inflate accepts any amount, and can take a circle out of validity.
		Circle shrunk(1.0f, 2.0f, 3.0f);
		shrunk.inflate(-4.0f);
		CHECK_FALSE(shrunk.is_valid());
	}
	TEST_CASE("every circle predicate answers false for a negative or nonfinite radius or centre")
	{
		// Each shape below lies within 3 of the origin, so a circle of
		// radius 3 there meets all of them. Each predicate squares the
		// radius, so without the validity check -3 would act as 3.
		const Circle reference(0.0f, 0.0f, 3.0f);
		const Circle partner(5.0f, 0.0f, 3.0f);
		const Triangle triangle(Point2F(1.0f, 0.0f), Point2F(5.0f, 0.0f), Point2F(1.0f, 5.0f));
		const Quad quad(RectangleF(1.0f, -1.0f, 4.0f, 2.0f));
		const Segment segment(Point2F(1.0f, -5.0f), Point2F(1.0f, 5.0f));
		const Point2F point(1.0f, 0.0f);
		const RectangleF rectangle(1.0f, -1.0f, 4.0f, 2.0f);
		const RectangleRotated rectangle_rotated(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

		Point2F p;
		CHECK(circles_intersect(reference, partner));
		CHECK(circle_triangle_intersect(reference, triangle));
		CHECK(circle_triangle_intersect(reference, triangle, p));
		CHECK(circle_quad_intersect(reference, quad));
		CHECK(circle_segment_intersect(reference, segment));
		CHECK(circle_segment_intersect(reference, segment, p));
		CHECK(circle_point_intersect(reference, point));
		CHECK(rectangle_circle_intersect(rectangle, reference));
		CHECK(rectangle_circle_intersect(rectangle, reference, p));
		CHECK(circle_rectangle_rotated_intersect(reference, rectangle_rotated));

		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float infinity = std::numeric_limits<float>::infinity();
		const Circle invalid_circles[] = {
			Circle(0.0f, 0.0f, -3.0f),
			Circle(nan, 0.0f, 3.0f),
			Circle(0.0f, nan, 3.0f),
			Circle(0.0f, 0.0f, nan),
			Circle(infinity, 0.0f, 3.0f),
			Circle(0.0f, -infinity, 3.0f),
			Circle(0.0f, 0.0f, infinity),
		};

		const Point2F untouched(-7.0f, -8.0f);
		for (const Circle& invalid : invalid_circles)
		{
			CHECK_FALSE(circles_intersect(invalid, partner));
			CHECK_FALSE(circles_intersect(partner, invalid));
			CHECK_FALSE(circles_intersect(invalid, invalid));
			CHECK_FALSE(circle_triangle_intersect(invalid, triangle));
			CHECK_FALSE(circle_quad_intersect(invalid, quad));
			CHECK_FALSE(circle_segment_intersect(invalid, segment));
			CHECK_FALSE(circle_point_intersect(invalid, point));
			CHECK_FALSE(rectangle_circle_intersect(rectangle, invalid));
			CHECK_FALSE(circle_rectangle_rotated_intersect(invalid, rectangle_rotated));

			p = untouched;
			CHECK_FALSE(circle_triangle_intersect(invalid, triangle, p));
			CHECK(p == untouched);
			CHECK_FALSE(circle_segment_intersect(invalid, segment, p));
			CHECK(p == untouched);
			CHECK_FALSE(rectangle_circle_intersect(rectangle, invalid, p));
			CHECK(p == untouched);
		}

		// Two circles of radius -3 five apart: the sum of radii squares to
		// 36, more than the 25 between their centres.
		CHECK_FALSE(circles_intersect(Circle(0.0f, 0.0f, -3.0f),
			Circle(5.0f, 0.0f, -3.0f)));
	}
	TEST_CASE("two triangles intersect when they overlap, nest or touch")
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
		b = Triangle(Point2F(10.0f - epsilon_f, 0.0f), Point2F(20.0f, 0.0f),
			Point2F(10.0f - epsilon_f, 10.0f));
		CHECK(triangles_intersect(a, b));

		// a and b are just not touching
		b = Triangle(Point2F(10.0f + epsilon_f, 0.0f), Point2F(20.0f, 0.0f),
			Point2F(10.0f + epsilon_f, 10.0f));
		CHECK_FALSE(triangles_intersect(a, b));
	}
	TEST_CASE("a triangle and a quad intersect when they overlap, nest or touch")
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
		q = Quad(RectangleF(10.0f - epsilon_f, 0.0f, 10.0f, 10.0f));
		CHECK(triangle_quad_intersect(a, q));

		// a and q are just not touching
		q = Quad(RectangleF(10.0f + epsilon_f, 0.0f, 10.0f, 10.0f));
		CHECK_FALSE(triangle_quad_intersect(a, q));
	}
	TEST_CASE("a triangle and a segment intersect when they cross, nest or touch")
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
		s = Segment(Point2F(10.0f - epsilon_f, 0.0f), Point2F(20.0f, 0.0f));
		CHECK(triangle_segment_intersect(a, s));

		// a and s are just not touching
		s = Segment(Point2F(10.0f + epsilon_f, 0.0f), Point2F(20.0f, 0.0f));
		CHECK_FALSE(triangle_segment_intersect(a, s));
	}
	TEST_CASE("a triangle and a rotated rectangle intersect when they overlap, nest or touch")
	{
		RectangleRotated rr(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

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
		t = Triangle(Vector2F::direction_down_right * (10.0f - epsilon_f),
			Vector2F::direction_down_right * 20.0f,
			Vector2F::direction_down_right * 20.0f + Vector2F::direction_up_right * 10.0f);
		CHECK(triangle_rectangle_rotated_intersect(t, rr));

		// t and rr are just not touching
		t = Triangle(Vector2F::direction_down_right * (10.0f + epsilon_f_100),
			Vector2F::direction_down_right * 20.0f,
			Vector2F::direction_down_right * 20.0f + Vector2F::direction_up_right * 10.0f);
		CHECK_FALSE(triangle_rectangle_rotated_intersect(t, rr));
	}
	TEST_CASE("two quads intersect when they overlap, nest or touch")
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
		b = Quad(RectangleF(10.0f - epsilon_f, 0.0f, 10.0f, 10.0f));
		CHECK(quads_intersect(a, b));

		// a and b are just not touching
		b = Quad(RectangleF(10.0f + epsilon_f_2, 0.0f, 10.0f, 10.0f));
		CHECK_FALSE(quads_intersect(a, b));

		// 2 identical quads
		CHECK(quads_intersect(a, a));
	}
	TEST_CASE("a quad and a segment intersect when they cross, nest or touch")
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
		s = Segment(Point2F(10.0f - epsilon_f, 0.0f), Point2F(20.0f, 0.0f));
		CHECK(quad_segment_intersect(a, s));

		// a and s are just not touching
		s = Segment(Point2F(10.0f + epsilon_f, 0.0f), Point2F(20.0f, 0.0f));
		CHECK_FALSE(quad_segment_intersect(a, s));
	}
	TEST_CASE("a point inside a quad or on its boundary intersects it")
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
	TEST_CASE("a quad and a rotated rectangle intersect when they overlap, nest or touch")
	{
		RectangleRotated rr(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

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
		q = Quad(RectangleF(Vector2F::direction_down_right * (10.0f - epsilon_f),
			Vector2F(10.0f, 10.0f)));
		CHECK(quad_rectangle_rotated_intersect(q, rr));

		// q and rr are just not touching
		q = Quad(RectangleF(Vector2F::direction_down_right * (10.0f + epsilon_f_100),
			Vector2F(10.0f, 10.0f)));
		CHECK_FALSE(quad_rectangle_rotated_intersect(q, rr));
	}
	TEST_CASE("two segments intersect only where they cross, whichever way round they are written")
	{
		Segment a(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));
		Segment b(Point2F(5.0f, -5.0f), Point2F(5.0f, 10.0f));
		CHECK(segments_intersect(a, b));
		b = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
		CHECK_FALSE(segments_intersect(a, b));
		// a and b are just touching
		b = Segment(Point2F(5.0f, -5.0f), Point2F(5.0f, epsilon_f));
		CHECK(segments_intersect(a, b));
		// a and b are just not touching
		b = Segment(Point2F(5.0f, -5.0f), Point2F(5.0f, -epsilon_f));
		CHECK_FALSE(segments_intersect(a, b));

		// Two identical segments do not CROSS, and this predicate only
		// answers about crossing. An `a == b` shortcut answering true makes
		// the result depend on which end the caller wrote first: Segment's
		// operator== is direction sensitive, so the identical segment matches
		// and the reversed one does not, for the same two points.
		//
		// Whatever the answer is, it has to be the same for both.
		const Segment same(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f));
		const Segment reversed(Point2F(10.0f, 0.0f), Point2F(0.0f, 0.0f));

		CHECK(segments_intersect(a, same) ==
			segments_intersect(a, reversed));
		CHECK_FALSE(segments_intersect(a, same));
		CHECK_FALSE(segments_intersect(a, reversed));

		// The shapes built on this are unaffected, because a flush contact
		// is caught by their containment pass rather than by this.
		const Triangle t0(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(0.0f, 10.0f));
		CHECK(triangles_intersect(t0, t0));
	}
	TEST_CASE("a segment crossing or inside a rotated rectangle intersects it")
	{
		RectangleRotated rr(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

		Segment s(Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f));
		CHECK(segment_rectangle_rotated_intersect(s, rr));

		s = Segment(Point2F(500.0f, 500.0f), Point2F(1500.0f, 500.0f));
		CHECK_FALSE(segment_rectangle_rotated_intersect(s, rr));

		// s is inside rr
		s = Segment(Point2F(2.0f, 2.0f), Point2F(3.0f, 3.0f));
		CHECK(segment_rectangle_rotated_intersect(s, rr));
	}
	TEST_CASE("a segment through opposite corners meets the shape, whichever way it runs")
	{
		// (-5,-5) to (15,15) enters the square (0,0)..(10,10) at one corner
		// and leaves at the other, through the whole of its interior. Both
		// endpoints are outside and every crossing is at a vertex, which the
		// proper segment primitive excludes by contract - so a
		// containment-plus-crossing test answers false to a traversal
		// (docs/review/gpt6/README.md, G6-05). Each of the three shape
		// routines, each direction.
		const Segment across(Point2F(-5.0f, -5.0f), Point2F(15.0f, 15.0f));
		const Segment back(Point2F(15.0f, 15.0f), Point2F(-5.0f, -5.0f));

		const Quad quad(RectangleF(0.0f, 0.0f, 10.0f, 10.0f));
		CHECK(quad_segment_intersect(quad, across));
		CHECK(quad_segment_intersect(quad, back));

		// An axis-aligned RectangleRotated over the same square.
		const RectangleRotated rotated(Point2F(5.0f, 5.0f),
			Vector2F::direction_right, Vector2F::direction_down,
			Vector2F(5.0f, 5.0f));
		CHECK(segment_rectangle_rotated_intersect(across, rotated));
		CHECK(segment_rectangle_rotated_intersect(back, rotated));

		// The triangle's version: through the right-angle corner and out
		// through the hypotenuse's far vertex is one edge, so (0,0) to
		// (10,0) along it; the traversal is corner (0,0) to corner (10,0)
		// with both ends past them.
		const Triangle triangle(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(0.0f, 10.0f));
		CHECK(triangle_segment_intersect(triangle,
			Segment(Point2F(-5.0f, 0.0f), Point2F(15.0f, 0.0f))));
		CHECK(triangle_segment_intersect(triangle,
			Segment(Point2F(15.0f, 0.0f), Point2F(-5.0f, 0.0f))));
		// And a vertex-to-vertex traversal with a proper interior: (0,10)
		// to (10,0) is the hypotenuse; (-5,15) to (15,-5) runs along it.
		CHECK(triangle_segment_intersect(triangle,
			Segment(Point2F(-5.0f, 15.0f), Point2F(15.0f, -5.0f))));

		// A near miss stays a miss: one unit outside the corner, parallel to
		// the diagonal.
		CHECK_FALSE(quad_segment_intersect(quad,
			Segment(Point2F(-5.0f, -4.0f), Point2F(-1.0f, 0.0f))));
		CHECK_FALSE(segment_rectangle_rotated_intersect(
			Segment(Point2F(-5.0f, -4.0f), Point2F(-1.0f, 0.0f)), rotated));
	}
	TEST_CASE("a point inside a rotated rectangle or on its boundary intersects it")
	{
		RectangleRotated rr(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

		Point2F p(5.0f, 5.0f);
		CHECK(point_rectangle_rotated_intersect(p, rr));

		p = Point2F(15.0f, 15.0f);
		CHECK_FALSE(point_rectangle_rotated_intersect(p, rr));

		// p is on the edge of rr
		p = Vector2F::direction_down_right * 10.0f;
		CHECK(point_rectangle_rotated_intersect(p, rr));

		// p is just outside the edge of rr
		p = Vector2F::direction_down_right * (10.0f + epsilon_f_100);
		CHECK_FALSE(point_rectangle_rotated_intersect(p, rr));

		// p is on the corner of rr
		p = Vector2F::direction_down_right * 10.0f + Vector2F::direction_up_right * 10.0f;
		CHECK(point_rectangle_rotated_intersect(p, rr));

		// p is just outside the corner of rr
		p = Vector2F::direction_down_right * (10.0f + epsilon_f_100) +
			Vector2F::direction_up_right * (10.0f + epsilon_f_100);
		CHECK_FALSE(point_rectangle_rotated_intersect(p, rr));

		// p is inside rr
		p = Point2F(2.0f, 2.0f);
		CHECK(point_rectangle_rotated_intersect(p, rr));
	}
	TEST_CASE("two rotated rectangles intersect when they overlap, nest or touch")
	{
		RectangleRotated a(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));

		RectangleRotated b(Vector2F::direction_down_right * 5.0f,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));
		CHECK(rectangles_rotated_intersect(a, b));

		b = RectangleRotated(Vector2F::direction_down_right * 200.0f,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));
		CHECK_FALSE(rectangles_rotated_intersect(a, b));

		// b is inside a
		b = RectangleRotated(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(2.0f, 2.0f));
		CHECK(rectangles_rotated_intersect(a, b));

		// a is inside b
		b = RectangleRotated(Vector2F::zero,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(40.0f, 40.0f));
		CHECK(rectangles_rotated_intersect(a, b));

		// a and b are just touching
		b = RectangleRotated(Vector2F::direction_down_right * 20.0f,
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));
		CHECK(rectangles_rotated_intersect(a, b));

		// a and b are just not touching
		b = RectangleRotated(Vector2F::direction_down_right * (20.0f + epsilon_f_100),
			Vector2F::direction_up_right,
			Vector2F::direction_down_right, Vector2F(10.0f, 10.0f));
		CHECK_FALSE(rectangles_rotated_intersect(a, b));

		// 2 identical rectangles
		CHECK(rectangles_rotated_intersect(a, a));
	}
}

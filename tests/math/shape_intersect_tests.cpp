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

using namespace mattmath;

constexpr float EPSILON_F = 0.000001f;
constexpr float EPSILON_F_2 = 0.000002f;
constexpr float EPSILON_F_100 = 0.0001f;

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
		b = RectangleF(10.0f - EPSILON_F, 10.0f, 10.0f, 10.0f);
		CHECK(a.intersects(b));

		// a and b are just not touching
		b = RectangleF(10.0f + EPSILON_F, 10.0f, 10.0f, 10.0f);
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

		// Two identical segments do not CROSS, and this predicate only
		// answers about crossing. It used to say true through an `a == b`
		// shortcut, which made the answer depend on which end the caller
		// wrote first: Segment's operator== is direction sensitive, so the
		// identical segment matched and the reversed one did not, for the
		// same two points.
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
}

#include <doctest/doctest.h>

#include "engine/math/matt_math.h"

#include <cmath>

using namespace mattmath;

TEST_SUITE("Triangle")
{
	TEST_CASE("a triangle's angles are its interior angles, and they sum to PI")
	{
		// They were taken between the two edge directions meeting at each
		// vertex, which is PI minus the interior angle - so an equilateral
		// triangle reported three 120-degree corners and angles() summed
		// to 2*PI. A right angle is its own supplement, which is why the
		// only consumer never noticed.
		const Triangle right(Point2F(0.0f, 0.0f), Point2F(0.0f, 10.0f),
			Point2F(10.0f, 0.0f));

		CHECK(are_equal(right.angle_0(), PI_OVER_2));
		CHECK(are_equal(right.angle_1(), PI / 4.0f));
		CHECK(are_equal(right.angle_2(), PI / 4.0f));

		CHECK(are_equal(right.angle_0() + right.angle_1() + right.angle_2(),
			PI));

		// Equilateral: 60 degrees at every corner, not 120.
		const float height = 10.0f * std::sqrt(3.0f) / 2.0f;
		const Triangle equilateral(Point2F(0.0f, 0.0f),
			Point2F(10.0f, 0.0f), Point2F(5.0f, height));

		CHECK(are_equal(equilateral.angle_0(), PI / 3.0f));
		CHECK(are_equal(equilateral.angle_1(), PI / 3.0f));
		CHECK(are_equal(equilateral.angle_2(), PI / 3.0f));
	}
	TEST_CASE("the hypotenuse is the side opposite the right angle, not the one beside it")
	{
		// find_hypotenuse answers with the VERTEX holding the right angle,
		// and both accessors used it to index edges() directly. Edge n
		// runs from vertex n, so the side opposite vertex v is edge v + 1
		// and they were returning a leg every time.
		const TriangleRightAxisAligned tri(Point2F(0.0f, 0.0f),
			Point2F(0.0f, 10.0f), Point2F(10.0f, 0.0f));

		const Segment h = tri.hypotenuse();

		// The long side: 14.14, against legs of 10.
		CHECK(are_equal(h.length(), std::sqrt(200.0f)));

		// And it is the side that does not touch the right-angled corner.
		CHECK(h.point_0 != Point2F(0.0f, 0.0f));
		CHECK(h.point_1 != Point2F(0.0f, 0.0f));

		// Falling from (0,10) to (10,0) is a gradient of -1.
		CHECK(are_equal(tri.hypotenuse_gradient(), -1.0f));
	}
	TEST_CASE("a default Triangle contains nothing, and collides with nothing")
	{
		// Triangle() is three copies of Vector2F::ZERO, and its
		// constructor validates nothing - narrow_phase.h says so in
		// writing and guards itself accordingly. Every edge vector is
		// (0, 0), so every signed area is exactly zero, and a predicate
		// that accepts by falling past both sign branches called that
		// "inside": the degenerate triangle contained every point in the
		// plane, and triangles_intersect returned true against it for any
		// triangle at all, through its containment pass.
		const Triangle degenerate;

		CHECK_FALSE(degenerate.contains(Point2F(1000.0f, 1000.0f)));
		CHECK_FALSE(degenerate.contains(Point2F::ZERO));

		// Deliberately away from the origin. A real triangle with a vertex
		// AT the origin genuinely touches the degenerate one, and would
		// pass the containment test for a reason that has nothing to do
		// with the defect under test.
		const Triangle real(Point2F(100.0f, 100.0f), Point2F(110.0f, 100.0f),
			Point2F(100.0f, 110.0f));

		CHECK_FALSE(triangles_intersect(real, degenerate));
		CHECK_FALSE(triangles_intersect(degenerate, real));

		// Against itself. This one needed the `a == b` shortcut in
		// segments_intersect to go as well: all three of a degenerate
		// triangle's edges are the same zero-length segment, so the
		// shortcut matched and reported a crossing.
		CHECK_FALSE(triangles_intersect(degenerate, degenerate));
	}
}

#include <doctest/doctest.h>

#include "engine/math/matt_math.h"

#include <cmath>

using namespace mattmath;

TEST_SUITE("Inflate")
{
	TEST_CASE("inflate never leaves a vertex outside the shape it grew from")
	{
		// The existing "an inflated shape contains the shape it grew from"
		// case only ever uses well-formed triangles. These are the two
		// shapes where the mitre solve failed the contract outright.

		SUBCASE("a collinear triangle is left alone rather than mangled")
		{
			// No interior means no outward: the centroid is on the same
			// line as every vertex, so the orientation test scored exactly
			// zero for all three edges and no normal was flipped. The
			// result was {(0,1), (5,1), (10,-1)}, which contains neither
			// (0,0) nor (10,0).
			Triangle collinear(Point2F(0.0f, 0.0f), Point2F(5.0f, 0.0f),
				Point2F(10.0f, 0.0f));
			const Triangle before = collinear;

			collinear.inflate(1.0f);

			CHECK(collinear == before);
		}

		SUBCASE("a needle's tip is not pushed sideways off its own mitre")
		{
			// The determinant at the tip is -2e-5, inside EPSILON, so the
			// parallel branch fired and moved the tip one unit UP. The
			// edges are not parallel: they double back, and they meet a
			// long way to the left.
			const Point2F tip(0.0f, 0.0f);
			Triangle needle(tip, Point2F(100.0f, 0.001f),
				Point2F(100.0f, -0.001f));

			needle.inflate(1.0f);

			CHECK(triangle_point_intersect(needle, tip));

			// Every original vertex, not just the awkward one.
			CHECK(triangle_point_intersect(needle, Point2F(100.0f, 0.001f)));
			CHECK(triangle_point_intersect(needle, Point2F(100.0f, -0.001f)));

			// The mitre goes outward, which is the direction that matters.
			CHECK(needle.point_0().x < tip.x);
		}

		SUBCASE("a straight-through vertex still takes the parallel push")
		{
			// The case the old guard was written for, and the one it got
			// right: a quad with three collinear points along one side.
			// Its determinant is also near zero, but the normals agree in
			// direction, so the straight push is the correct limit.
			Quad q(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
				Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f));

			q.inflate(1.0f);

			CHECK(quad_point_intersect(q, Point2F(0.0f, 0.0f)));
			CHECK(quad_point_intersect(q, Point2F(10.0f, 10.0f)));
			CHECK(quad_point_intersect(q, Point2F(5.0f, 5.0f)));
		}
	}
	TEST_CASE("inflate moves every edge out by the full amount, not by a cosine")
	{
		// The property the radial version could not hold. Displacing a
		// vertex by `amount` along its ray from the centroid moves the
		// edges meeting there by only amount * cos(angle), so a sharp
		// corner under-inflated badly and a right angle by a factor of
		// 1/sqrt(2).
		//
		// Measured from a FIXED interior point, captured before the
		// inflation. The centroid of the grown polygon is not the centroid
		// of the original, so measuring from the live centre compares
		// against a reference that moved - which is a bug in the test, not
		// in the arithmetic, and cost one confused run to find.
		const auto edge_distance = [](const Triangle& t, int i,
			const Point2F& from)
		{
			const Point2F a = t.points[i];
			const Point2F b = t.points[(i + 1) % 3];
			const Vector2F normal = Vector2F(-(b.y - a.y), b.x - a.x)
				.normalized();
			return std::abs(Vector2F::dot(from - a, normal));
		};

		// A deliberately sharp triangle - the case the old form was worst
		// on.
		Triangle t(Point2F(0.0f, 0.0f), Point2F(100.0f, 0.0f),
			Point2F(90.0f, 20.0f));

		const Point2F reference = t.center();

		float before[3];
		for (int i = 0; i < 3; i++)
		{
			before[i] = edge_distance(t, i, reference);
		}

		constexpr float AMOUNT = 5.0f;
		t.inflate(AMOUNT);

		for (int i = 0; i < 3; i++)
		{
			CAPTURE(i);
			CHECK(edge_distance(t, i, reference)
				== doctest::Approx(before[i] + AMOUNT).epsilon(0.001));
		}
	}
	TEST_CASE("an inflated shape contains the shape it grew from")
	{
		// The direction of the error is the contract. A collider that
		// grows by less than asked lets things visibly interpenetrate
		// while collision correctly reports no touch.
		const Triangle original(Point2F(10.0f, 10.0f), Point2F(60.0f, 12.0f),
			Point2F(20.0f, 40.0f));

		Triangle grown = original;
		grown.inflate(3.0f);

		for (int i = 0; i < 3; i++)
		{
			CAPTURE(i);
			CHECK(triangle_point_intersect(grown, original.points[i]));
		}

		// And a quad, which shares the implementation.
		const Quad square(Point2F(0.0f, 0.0f), Point2F(20.0f, 0.0f),
			Point2F(20.0f, 20.0f), Point2F(0.0f, 20.0f));
		Quad bigger = square;
		bigger.inflate(2.0f);

		// A square inflated by 2 is the square grown by 2 on every side.
		CHECK(bigger.point_0().x == doctest::Approx(-2.0f));
		CHECK(bigger.point_0().y == doctest::Approx(-2.0f));
		CHECK(bigger.point_2().x == doctest::Approx(22.0f));
		CHECK(bigger.point_2().y == doctest::Approx(22.0f));
	}
}

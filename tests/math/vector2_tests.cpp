#include <doctest/doctest.h>

#include "engine/math/matt_math.h"
#include "engine/math/ericson_math.h"

#include <cmath>

using namespace mattmath;

TEST_SUITE("Vector2")
{
	TEST_CASE("unary minus reverses a vector")
	{
		// The collision module reverses a contact normal constantly and
		// wrote it out by hand in two different spellings.
		const Vector2F v(3.0f, -4.0f);

		CHECK(-v == Vector2F(-3.0f, 4.0f));
		CHECK(-(-v) == v);
		CHECK(v + (-v) == Vector2F::ZERO);
		CHECK(-Vector2F::ZERO == Vector2F::ZERO);

		// It agrees with the spelling it replaces.
		CHECK(-v == v * -1.0f);
	}
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
	TEST_CASE("angle_between survives a zero-length vector and its own rounding")
	{
		// Zero in, zero out - the contract normalized() already keeps.
		// This used to be acos(0/0), and the NaN surfaced two call levels
		// away as "Triangle is not a right triangle", thrown about a
		// triangle that was one.
		CHECK(Vector2F::angle_between(Vector2F::ZERO,
			Vector2F(1.0f, 0.0f)) == 0.0f);
		CHECK(Vector2F::angle_between(Vector2F(1.0f, 0.0f),
			Vector2F::ZERO) == 0.0f);

		// Parallel and antiparallel are the two places the quotient lands
		// exactly on the edge of acos's domain, so rounding can push it
		// outside and produce a NaN from arithmetic that was never wrong
		// by more than an ulp.
		const Vector2F v(3.0f, 4.0f);
		CHECK(Vector2F::angle_between(v, v) == doctest::Approx(0.0f));
		CHECK(Vector2F::angle_between(v, v * -1.0f)
			== doctest::Approx(PI));
		CHECK_FALSE(std::isnan(Vector2F::angle_between(v, v)));

		// A right angle, which is what the triangle predicates ask for.
		CHECK(Vector2F::angle_between(Vector2F(1.0f, 0.0f),
			Vector2F(0.0f, 1.0f)) == doctest::Approx(PI_OVER_2));
	}
}

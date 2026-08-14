#include <doctest/doctest.h>

#include "engine/math/matrix3x2f.h"
#include "engine/math/scalar.h"
#include "engine/math/vector2f.h"

#include <cmath>
#include <limits>
#include <stdexcept>

using namespace mattmath;

namespace
{
	// Element by element, with a tolerance. operator== is exact on purpose, so
	// two transforms built by different routes to the same place compare
	// unequal there and have to be compared here instead.
	void check_close(const Matrix3x2F& actual, const Matrix3x2F& expected)
	{
		CHECK(actual.m11 == doctest::Approx(expected.m11));
		CHECK(actual.m12 == doctest::Approx(expected.m12));
		CHECK(actual.m21 == doctest::Approx(expected.m21));
		CHECK(actual.m22 == doctest::Approx(expected.m22));
		CHECK(actual.m31 == doctest::Approx(expected.m31));
		CHECK(actual.m32 == doctest::Approx(expected.m32));
	}

	void check_close(const Vector2F& actual, const Vector2F& expected)
	{
		CHECK(actual.x == doctest::Approx(expected.x));
		CHECK(actual.y == doctest::Approx(expected.y));
	}
}

TEST_SUITE("Matrix3x2F")
{
	TEST_CASE("a default-constructed transform is the identity")
	{
		// The value a composition starts from, and the reason the default
		// constructor is worth having at all.
		CHECK(Matrix3x2F() == Matrix3x2F::identity);

		const Point2F point(3.0f, -4.0f);

		// Exact, not approximate: the identity's arithmetic is
		// x * 1 + y * 0 + 0, which rounds to nothing.
		CHECK(Matrix3x2F::identity.transform_point(point) == point);
		CHECK(Matrix3x2F::identity.transform_vector(point) == point);
	}
	TEST_CASE("translation moves points and leaves directions alone")
	{
		// The whole reason transform_point and transform_vector are two
		// functions. A position moves with the world; a difference between two
		// positions does not.
		const Matrix3x2F move = Matrix3x2F::translation(Vector2F(10.0f, 5.0f));

		CHECK(move.transform_point(Point2F(1.0f, 2.0f)) ==
			Point2F(11.0f, 7.0f));
		CHECK(move.transform_vector(Vector2F(1.0f, 2.0f)) ==
			Vector2F(1.0f, 2.0f));

		// Stated the other way round, which is the property callers rely on:
		// translating both ends of a segment leaves the segment's direction
		// and length untouched.
		const Point2F a(3.0f, 4.0f);
		const Point2F b(-7.0f, 1.0f);
		CHECK(move.transform_point(b) - move.transform_point(a) == b - a);
	}
	TEST_CASE("translation() reads back what a composition arrived at")
	{
		const Matrix3x2F transform =
			Matrix3x2F::translation(Vector2F(3.0f, 4.0f)) *
			Matrix3x2F::scaling(2.0f);

		// The scale applies to the translation that came before it, which is
		// the ordering the free operator* documents.
		check_close(transform.translation(), Vector2F(6.0f, 8.0f));

		// It is the bottom row, and the bottom row is where the origin lands.
		check_close(transform.translation(),
			transform.transform_point(Point2F::ZERO));
	}
	TEST_CASE("rotation agrees with the angle convention Vector2F already has")
	{
		// The identity that keeps this type from inventing a second sense of
		// rotation. If either side is ever rewritten, this is what notices.
		const float angle = 0.7f;
		const Matrix3x2F turn = Matrix3x2F::rotation(angle);

		check_close(turn.transform_vector(Vector2F::DIRECTION_RIGHT),
			Vector2F::unit_vec_from_angle(angle));

		CHECK(turn.transform_vector(Vector2F::DIRECTION_RIGHT).angle() ==
			doctest::Approx(angle));
	}
	TEST_CASE("rotation turns a quarter circle without changing length")
	{
		const Matrix3x2F quarter = Matrix3x2F::rotation(PI_OVER_2);

		// In maths axes this is counter-clockwise; on a y-down screen the same
		// number reads as clockwise. The arithmetic is what is pinned here.
		check_close(quarter.transform_vector(Vector2F::DIRECTION_RIGHT),
			Vector2F(0.0f, 1.0f));
		check_close(quarter.transform_vector(Vector2F(0.0f, 1.0f)),
			Vector2F(-1.0f, 0.0f));

		// A rotation is rigid: it moves things without resizing them, which is
		// a determinant of exactly one.
		const Vector2F v(3.0f, -4.0f);
		CHECK(Matrix3x2F::rotation(1.234f).transform_vector(v).length() ==
			doctest::Approx(v.length()));
		CHECK(Matrix3x2F::rotation(1.234f).determinant() ==
			doctest::Approx(1.0f));
	}
	TEST_CASE("rotation is about the origin, and moves everything else")
	{
		const Matrix3x2F quarter = Matrix3x2F::rotation(PI_OVER_2);

		// The origin is the fixed point, exactly - there is nothing in that
		// arithmetic to round.
		CHECK(quarter.transform_point(Point2F::ZERO) == Point2F::ZERO);

		// Every other position swings around it. A sprite that orbits when it
		// was meant to spin is this, applied to a position that is not at the
		// origin; the fix is composition, in the case below.
		check_close(quarter.transform_point(Point2F(10.0f, 0.0f)),
			Point2F(0.0f, 10.0f));
	}
	TEST_CASE("scaling resizes, mirrors, and reports itself in the determinant")
	{
		check_close(Matrix3x2F::scaling(Vector2F(2.0f, 3.0f))
			.transform_point(Point2F(4.0f, 5.0f)), Point2F(8.0f, 15.0f));

		// The uniform overload is the two-axis one with the same number twice,
		// and nothing more.
		CHECK(Matrix3x2F::scaling(2.5f) ==
			Matrix3x2F::scaling(Vector2F(2.5f, 2.5f)));

		// The determinant is the area scale: 2 x 3 makes everything six times
		// larger.
		CHECK(Matrix3x2F::scaling(Vector2F(2.0f, 3.0f)).determinant() ==
			doctest::Approx(6.0f));

		// Negative on a mirror, because handedness has flipped. This is the
		// sign, not a failure - Vector2F::cross makes the same distinction.
		CHECK(Matrix3x2F::scaling(Vector2F(-1.0f, 1.0f)).determinant() ==
			doctest::Approx(-1.0f));
	}
	TEST_CASE("composition applies left to right, and does not commute")
	{
		const Matrix3x2F turn = Matrix3x2F::rotation(PI_OVER_2);
		const Matrix3x2F move = Matrix3x2F::translation(Vector2F(10.0f, 0.0f));

		// Turn first, then move: the point ends up ten to the right of where
		// the rotation left it.
		check_close((turn * move).transform_point(Point2F(1.0f, 0.0f)),
			Point2F(10.0f, 1.0f));

		// Move first, then turn: the rotation carries the offset with it.
		check_close((move * turn).transform_point(Point2F(1.0f, 0.0f)),
			Point2F(0.0f, 11.0f));

		// Which is to say the two are different transforms. Order is meaning
		// here, exactly as it is for the two actions.
		CHECK(turn * move != move * turn);
	}
	TEST_CASE("a composition is the transforms applied one after another")
	{
		// The property that makes composing worth doing at all: one matrix
		// through one call is the same answer as three matrices through three.
		const Matrix3x2F first = Matrix3x2F::translation(Vector2F(3.0f, -2.0f));
		const Matrix3x2F second = Matrix3x2F::rotation(0.4f);
		const Matrix3x2F third = Matrix3x2F::scaling(Vector2F(2.0f, 0.5f));

		const Point2F point(7.0f, 11.0f);

		check_close((first * second * third).transform_point(point),
			third.transform_point(
				second.transform_point(
					first.transform_point(point))));

		// And it associates, so a caller may pre-compose any run of them.
		check_close((first * second) * third, first * (second * third));
	}
	TEST_CASE("composition places a rotation about a point")
	{
		// The worked example in the header, executed. Turning something about
		// its own centre is the commonest manual transform there is, and it is
		// three composed factories rather than a fourth factory.
		const Point2F centre(100.0f, 50.0f);
		const Matrix3x2F spin_about_centre =
			Matrix3x2F::translation(-centre) *
			Matrix3x2F::rotation(PI_OVER_2) *
			Matrix3x2F::translation(centre);

		// The centre is the one point that does not move.
		check_close(spin_about_centre.transform_point(centre), centre);

		// Everything else turns about it: a point ten to the right of the
		// centre lands ten below it, by the quarter turn above.
		check_close(spin_about_centre.transform_point(
			centre + Vector2F(10.0f, 0.0f)), centre + Vector2F(0.0f, 10.0f));
	}
	TEST_CASE("operator*= is the free operator, in place")
	{
		const Matrix3x2F turn = Matrix3x2F::rotation(0.9f);
		const Matrix3x2F move = Matrix3x2F::translation(Vector2F(4.0f, 6.0f));

		Matrix3x2F accumulated = turn;
		accumulated *= move;

		CHECK(accumulated == turn * move);

		// Self-composition works: the implementation assigns through a
		// temporary rather than writing elements it still has to read.
		Matrix3x2F squared = turn;
		squared *= squared;
		check_close(squared, Matrix3x2F::rotation(1.8f));
	}
	TEST_CASE("inverse undoes the transform it came from")
	{
		const Matrix3x2F transform =
			Matrix3x2F::translation(Vector2F(3.0f, -2.0f)) *
			Matrix3x2F::rotation(0.4f) *
			Matrix3x2F::scaling(Vector2F(2.0f, 0.5f));

		check_close(transform * transform.inverse(), Matrix3x2F::identity);
		check_close(transform.inverse() * transform, Matrix3x2F::identity);

		// The round trip a caller actually makes: screen back to world.
		const Point2F point(37.0f, -19.0f);
		check_close(
			transform.inverse().transform_point(
				transform.transform_point(point)),
			point);
	}
	TEST_CASE("inverse refuses a transform that collapses the plane")
	{
		// A zero scale flattens everything onto a line, and every point on
		// that line came from infinitely many. There is no transform to
		// return, so it says so rather than handing back infinities (T6).
		CHECK_THROWS_AS(Matrix3x2F::scaling(Vector2F(0.0f, 1.0f)).inverse(),
			std::invalid_argument);
		CHECK_THROWS_AS(Matrix3x2F::scaling(0.0f).inverse(),
			std::invalid_argument);

		// The same collapse arrived at without a zero anywhere in it: two rows
		// that are multiples of each other span a line, not a plane.
		CHECK_THROWS_AS(Matrix3x2F(1.0f, 2.0f,
			2.0f, 4.0f,
			5.0f, 6.0f).inverse(), std::invalid_argument);

		// A transform that is already broken stays refused rather than
		// producing a plausible-looking matrix of zeroes.
		const float infinity = std::numeric_limits<float>::infinity();
		CHECK_THROWS_AS(Matrix3x2F::scaling(infinity).inverse(),
			std::invalid_argument);
		CHECK_THROWS_AS(Matrix3x2F::scaling(
			std::numeric_limits<float>::quiet_NaN()).inverse(),
			std::invalid_argument);
	}
	TEST_CASE("inverse accepts a scale far below EPSILON")
	{
		// The claim the header makes about tolerances, made executable. A
		// zoomed-far-out camera is a uniform scale of 0.005, whose determinant
		// is 2.5e-5 - a quarter of EPSILON. It is perfectly invertible, and a
		// guard written against EPSILON would refuse it.
		const Matrix3x2F zoomed_out = Matrix3x2F::scaling(0.005f);

		CHECK(zoomed_out.determinant() < EPSILON);

		const Matrix3x2F back = zoomed_out.inverse();
		CHECK(back.m11 == doctest::Approx(200.0f));
		CHECK(back.m22 == doctest::Approx(200.0f));

		check_close(back.transform_point(
			zoomed_out.transform_point(Point2F(640.0f, 360.0f))),
			Point2F(640.0f, 360.0f));
	}
	TEST_CASE("determinant is zero exactly when the transform is not invertible")
	{
		CHECK(Matrix3x2F::identity.determinant() == 1.0f);

		// Translation moves the plane without resizing it, so it costs the
		// determinant nothing - the bottom row is not part of it.
		CHECK(Matrix3x2F::translation(Vector2F(1000.0f, -1000.0f))
			.determinant() == 1.0f);

		CHECK(Matrix3x2F::scaling(Vector2F(3.0f, 0.0f)).determinant() == 0.0f);
	}
	TEST_CASE("equality is exact")
	{
		// Deliberately not a tolerance, and the case that says so: these two
		// transforms do the same thing and are not the same six floats.
		const Matrix3x2F built = Matrix3x2F::rotation(PI_OVER_2);
		const Matrix3x2F written = Matrix3x2F(0.0f, 1.0f,
			-1.0f, 0.0f,
			0.0f, 0.0f);

		CHECK(built != written);
		check_close(built, written);

		CHECK(built == Matrix3x2F::rotation(PI_OVER_2));
		CHECK_FALSE(built != Matrix3x2F::rotation(PI_OVER_2));
	}
}

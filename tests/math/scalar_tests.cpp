#include <doctest/doctest.h>

#include "engine/math/scalar.h"
#include "engine/math/vector2f.h"

using namespace labrador;

// The edge behaviour scalar.h and vector2f.h document for clamp and lerp,
// held to the letter: an inverted range is answered by the order of the two
// tests, and t is never clamped.
TEST_SUITE("Scalar")
{
	TEST_CASE("clamp keeps a value inside the range and moves one outside it")
	{
		CHECK(clamp(0.5f, 0.0f, 1.0f) == 0.5f);
		CHECK(clamp(-3.0f, 0.0f, 1.0f) == 0.0f);
		CHECK(clamp(7.0f, 0.0f, 1.0f) == 1.0f);

		// The bounds themselves are inside.
		CHECK(clamp(0.0f, 0.0f, 1.0f) == 0.0f);
		CHECK(clamp(1.0f, 0.0f, 1.0f) == 1.0f);
	}

	TEST_CASE("clamp with the floor above the ceiling tests the floor first")
	{
		// Below the floor gets the floor.
		CHECK(clamp(5.0f, 10.0f, 0.0f) == 10.0f);
		CHECK(clamp(-1.0f, 10.0f, 0.0f) == 10.0f);

		// Everything else gets the ceiling, the floor itself included.
		CHECK(clamp(10.0f, 10.0f, 0.0f) == 0.0f);
		CHECK(clamp(15.0f, 10.0f, 0.0f) == 0.0f);
	}

	TEST_CASE("lerp meets its endpoints and extrapolates outside [0, 1]")
	{
		CHECK(lerp(2.0f, 4.0f, 0.0f) == 2.0f);
		CHECK(lerp(2.0f, 4.0f, 1.0f) == 4.0f);
		CHECK(lerp(2.0f, 4.0f, 0.5f) == 3.0f);

		// Not clamped: past either end it keeps going along the same line.
		CHECK(lerp(0.0f, 10.0f, 1.5f) == 15.0f);
		CHECK(lerp(0.0f, 10.0f, -0.5f) == -5.0f);
	}

	TEST_CASE("Vector2F::lerp is the scalar lerp on each component")
	{
		const Vector2F a(0.0f, 10.0f);
		const Vector2F b(10.0f, 30.0f);

		CHECK(Vector2F::lerp(a, b, 0.0f) == a);
		CHECK(Vector2F::lerp(a, b, 1.0f) == b);
		CHECK(Vector2F::lerp(a, b, 0.5f) == Vector2F(5.0f, 20.0f));

		// And, like the scalar form, it extrapolates.
		CHECK(Vector2F::lerp(a, b, 2.0f) == Vector2F(20.0f, 50.0f));
		CHECK(Vector2F::lerp(a, b, -1.0f) == Vector2F(-10.0f, -10.0f));
	}
}

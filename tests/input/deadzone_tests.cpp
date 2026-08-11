#include <doctest/doctest.h>
#include "engine/input/gamepad.h"
#include "engine/math/vector2f.h"
using namespace artattack;
using namespace mattmath;

namespace DeadzoneTests
{
	TEST_SUITE("DeadzoneTests")
	{
		TEST_CASE("inside the deadzone is nothing")
		{
			CHECK(apply_deadzone(0.0f, 0.5f) == doctest::Approx(0.0f));
			CHECK(apply_deadzone(0.4f, 0.5f) == doctest::Approx(0.0f));
			CHECK(apply_deadzone(-0.4f, 0.5f) == doctest::Approx(0.0f));

			// On the line is inside it, so a stick resting exactly there does
			// not flicker between the two answers.
			CHECK(apply_deadzone(0.5f, 0.5f) == doctest::Approx(0.0f));
		}

		TEST_CASE("the live range is rescaled, not offset")
		{
			// This is the whole point of the function, and the defect it was
			// written for: the paint-shooter zeroed below the threshold and
			// passed the raw value above it, so the stick jumped from 0 to 0.5
			// and half its travel was unreachable.
			CHECK(apply_deadzone(0.5f, 0.5f) == doctest::Approx(0.0f));
			CHECK(apply_deadzone(0.75f, 0.5f) == doctest::Approx(0.5f));
			CHECK(apply_deadzone(1.0f, 0.5f) == doctest::Approx(1.0f));
		}

		TEST_CASE("the sign survives")
		{
			CHECK(apply_deadzone(-0.75f, 0.5f) == doctest::Approx(-0.5f));
			CHECK(apply_deadzone(-1.0f, 0.5f) == doctest::Approx(-1.0f));
		}

		TEST_CASE("a deadzone that cannot be applied passes the value through")
		{
			// Rather than dividing by zero, or by a negative.
			CHECK(apply_deadzone(0.3f, 0.0f) == doctest::Approx(0.3f));
			CHECK(apply_deadzone(0.3f, -1.0f) == doctest::Approx(0.3f));
			CHECK(apply_deadzone(0.3f, 1.0f) == doctest::Approx(0.3f));
			CHECK(apply_deadzone(0.3f, 2.0f) == doctest::Approx(0.3f));
		}

		TEST_CASE("the stick deadzone is round, not square")
		{
			// A per-axis deadzone lets a diagonal through that a straight push
			// of the same distance would not: (0.4, 0.4) is 0.57 from centre,
			// which is outside a 0.5 deadzone, and neither axis is.
			const Vector2F diagonal = apply_deadzone(Vector2F(0.4f, 0.4f), 0.5f);
			CHECK(diagonal.length() > 0.0f);

			// And it stops one that is inside it on both counts.
			const Vector2F small = apply_deadzone(Vector2F(0.3f, 0.3f), 0.5f);
			CHECK(small == Vector2F::ZERO);
		}

		TEST_CASE("the stick deadzone rescales the magnitude and keeps the angle")
		{
			const Vector2F pushed = apply_deadzone(Vector2F(0.0f, 1.0f), 0.5f);
			CHECK(pushed.x == doctest::Approx(0.0f));
			CHECK(pushed.y == doctest::Approx(1.0f));

			const Vector2F half = apply_deadzone(Vector2F(0.75f, 0.0f), 0.5f);
			CHECK(half.x == doctest::Approx(0.5f));
			CHECK(half.y == doctest::Approx(0.0f));

			// Direction untouched: a 45-degree push stays at 45 degrees, which
			// is what doing the scalar version to each axis would not give.
			const Vector2F angled = apply_deadzone(Vector2F(0.6f, 0.6f), 0.5f);
			CHECK(angled.x == doctest::Approx(angled.y));
		}

		TEST_CASE("a stick inside the deadzone is exactly zero")
		{
			// Callers test the result against ZERO to mean "nothing was asked
			// for", so it has to be exactly that and not merely small.
			CHECK(apply_deadzone(Vector2F::ZERO, 0.5f) == Vector2F::ZERO);
		}
	}
}

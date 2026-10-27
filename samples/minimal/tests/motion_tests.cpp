// The template's rules, run with no window and no device: this target links
// MinimalRules and doctest and nothing of the engine's but its maths, so it
// runs on a build machine with no display (CMakeLists.txt, MinimalTests).
//
// A copy keeps the shape and replaces the cases. One file is enough to start;
// list each new one in CMakeLists.txt beside this.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "samples/minimal/rules/motion.h"

using namespace labrador;

TEST_CASE("the stick and the keys add up to at most one unit")
{
	// Up and left on the keys is longer than a unit on its own, and is cut
	// back to one, pointing the same way.
	const Vector2F diagonal = combined_direction(Vector2F::zero,
		Vector2F(-1.0f, -1.0f));
	CHECK(diagonal.length() == doctest::Approx(1.0f));
	CHECK(diagonal.x == doctest::Approx(diagonal.y));

	// A small push of the stick with no key is left as it is, so it moves
	// slowly rather than jumping to full speed.
	const Vector2F nudge = combined_direction(Vector2F(0.25f, 0.0f),
		Vector2F::zero);
	CHECK(nudge.x == doctest::Approx(0.25f));
	CHECK(nudge.y == doctest::Approx(0.0f));
}

TEST_CASE("a step moves the centre and turns the greeting")
{
	const Motion start{ Vector2F(100.0f, 100.0f), 0.0f };

	// A whole second to the right on the keys.
	const Motion moved = step_motion(start, Vector2F::zero,
		Vector2F(1.0f, 0.0f), 1.0f);
	CHECK(moved.centre.x == doctest::Approx(100.0f + move_speed));
	CHECK(moved.centre.y == doctest::Approx(100.0f));
	CHECK(moved.spin == doctest::Approx(spin_speed));

	// Before it turns, the greeting is straight above its centre, +y being
	// down.
	const Vector2F above = greeting_position(start);
	CHECK(above.x == doctest::Approx(100.0f));
	CHECK(above.y == doctest::Approx(100.0f - orbit_radius));
}

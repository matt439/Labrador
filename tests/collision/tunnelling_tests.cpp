#include <doctest/doctest.h>

#include "engine/collision/tunnelling.h"
#include "engine/collision/narrow_phase.h"
#include "engine/math/rectanglef.h"

#include <limits>

using labrador::can_tunnel;
using labrador::max_safe_displacement;
using labrador::max_safe_speed;

TEST_CASE("axis-parallel boxes have a budget equal to their combined travel-axis extent")
{
	// A 20-unit projectile against a 10-unit wall is in contact over 30 units
	// of travel, so a step shorter than 30 cannot step over the contact.
	CHECK(max_safe_displacement(20.0f, 10.0f) == 30.0f);

	CHECK_FALSE(can_tunnel(29.0f, 20.0f, 10.0f));
	CHECK(can_tunnel(31.0f, 20.0f, 10.0f));

	// The boundary itself is not safe: at exactly the combined extent the two
	// touch at one instant, and touching is not overlapping (narrow_phase.h).
	CHECK(can_tunnel(30.0f, 20.0f, 10.0f));
}

TEST_CASE("a displacement that is not a number is treated as able to tunnel")
{
	// Written as the dangerous case rather than the safe one. Every comparison
	// against NaN is false, so a test phrased "is this safe" would answer yes.
	const float nan = std::numeric_limits<float>::quiet_NaN();

	CHECK(can_tunnel(nan, 20.0f, 10.0f));
}

TEST_CASE("speeds are converted against the step the application actually runs")
{
	// 30 units of contact at 60 steps a second is 1800 units a second.
	CHECK(max_safe_speed(20.0f, 10.0f, 1.0f / 60.0f) == doctest::Approx(1800.0f));

	// Halving the step doubles the boundary speed, which is the reason a speed cap
	// cannot be stated without naming the frame rate it assumes.
	CHECK(max_safe_speed(20.0f, 10.0f, 1.0f / 120.0f)
		== doctest::Approx(3600.0f));
}

TEST_CASE("a speed past the budget tunnels, and the budget names the remedies")
{
	// A worked example with round numbers. A 20-unit projectile moving at
	// 2000 units a second, stepped 60 times a second, moves 33.33 units a
	// step. Against a 10-unit wall the budget is 20 + 10 = 30, so the
	// projectile can pass through the wall without a step on which the two
	// overlap. T3 caps speeds so that nothing tunnels; a game meets that cap
	// with a slower projectile, a larger one, or thicker geometry, and these
	// are the numbers each remedy has to reach.
	constexpr float speed = 2000.0f;
	constexpr float projectile_extent = 20.0f;
	constexpr float wall_thickness = 10.0f;
	constexpr float step = 1.0f / 60.0f;

	CHECK(can_tunnel(speed * step, projectile_extent, wall_thickness));

	CHECK(max_safe_speed(projectile_extent, wall_thickness, step)
		== doctest::Approx(1800.0f));

	// So the speed has to come down to 1800, or the wall has to come up to
	// 13.34 units thick.
	CHECK(speed > max_safe_speed(projectile_extent, wall_thickness, step));
}

TEST_CASE("projected extents cannot rule out a diagonal corner crossing")
{
	const labrador::RectangleF fixed(0.0f, 0.0f, 1.0f, 1.0f);
	const labrador::RectangleF before(-1.1f, 0.8f, 1.0f, 1.0f);
	const labrador::RectangleF middle(-0.95f, 0.95f, 1.0f, 1.0f);
	const labrador::RectangleF after(-0.8f, 1.1f, 1.0f, 1.0f);
	const float displacement = (after.position() - before.position()).length();
	const float projected_extent = labrador::Vector2F(1.0f, 1.0f).length();

	// Outside the helper's axis-parallel assumptions, its numeric result is
	// only a heuristic. This finite counterexample must not become a claim
	// that a smaller displacement guarantees collision detection.
	CHECK_FALSE(can_tunnel(displacement, projected_extent, projected_extent));
	CHECK_FALSE(labrador::narrow_phase(fixed, before).has_value());
	CHECK(labrador::narrow_phase(fixed, middle).has_value());
	CHECK_FALSE(labrador::narrow_phase(fixed, after).has_value());
}

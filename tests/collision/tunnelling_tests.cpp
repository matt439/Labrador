#include <doctest/doctest.h>

#include "engine/collision/tunnelling.h"

#include <limits>

using artattack::can_tunnel;
using artattack::max_safe_displacement;
using artattack::max_safe_speed;

TEST_CASE("the budget is the combined extent along the direction of travel")
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

	// Halving the step halves what is safe, which is the reason a speed cap
	// cannot be stated without naming the frame rate it assumes.
	CHECK(max_safe_speed(20.0f, 10.0f, 1.0f / 120.0f)
		== doctest::Approx(3600.0f));
}

TEST_CASE("the shipping sniper exceeds the budget, and this is the record of it")
{
	// The numbers, from the tree rather than from memory:
	//   weapon_consts.h  DETAILS_SNIPER.starting_vel_length = 2000 units/sec
	//   projectile_consts.h  DETAILS_JET.col_rect_size      = 20 x 20 units
	//   main.cpp  options.target_fps                        = 60
	//   close_quarters.json  spawn_a_ceiling                = 300 x 10 units
	//
	// 2000 / 60 is 33.33 units a step against a budget of 20 + 10 = 30, so a
	// sniper shot fired at one of the 10-unit spawn ceilings passes through it
	// without a collision ever being tested. T3 says projectile speeds are
	// capped so nothing tunnels; this is the arithmetic showing that they are
	// not, and it is deliberately a failing-in-spirit fact pinned as a test
	// rather than a silent gap.
	//
	// The engine cannot fix this. The speed, the projectile size and the level
	// geometry are all game content, and every available remedy - a slower
	// sniper, a fatter bullet, thicker ceilings - changes how the game plays.
	// That is a decision for whoever owns the tuning, not for the commit that
	// found the arithmetic.
	//
	// So this test asserts the violation rather than the fix. It passes today
	// and it will fail the moment the content is corrected, which is the
	// point: the failure is the reminder to delete it, and until then nobody
	// can believe T3's cap exists by reading the philosophy alone.
	constexpr float SNIPER_SPEED = 2000.0f;
	constexpr float JET_EXTENT = 20.0f;
	constexpr float THINNEST_CEILING = 10.0f;
	constexpr float STEP = 1.0f / 60.0f;

	CHECK(can_tunnel(SNIPER_SPEED * STEP, JET_EXTENT, THINNEST_CEILING));

	CHECK(max_safe_speed(JET_EXTENT, THINNEST_CEILING, STEP)
		== doctest::Approx(1800.0f));

	// Which is the number the tuning would have to come down to, or the
	// geometry would have to come up to 13.34 units thick.
	CHECK(SNIPER_SPEED > max_safe_speed(JET_EXTENT, THINNEST_CEILING, STEP));
}

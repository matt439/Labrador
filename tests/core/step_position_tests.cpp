#include <doctest/doctest.h>

#include "engine/core/step_position.h"
#include "engine/core/step_timer.h"

#include <cstdint>

// What Application::step_position() publishes, with the clock taken out.
// application_loop_tests.cpp holds that the loop publishes it.

using namespace labrador;

TEST_CASE("step_position is the left-over share of one step")
{
	const StepPosition start = step_position(0, 0, 166666);
	CHECK(start.step == 0);
	CHECK(start.fraction == 0.0f);

	const StepPosition half = step_position(7, 50, 100);
	CHECK(half.step == 7);
	CHECK(half.fraction == 0.5f);

	const StepPosition quarter = step_position(1, 25, 100);
	CHECK(quarter.fraction == 0.25f);
}

TEST_CASE("step_position stays below one at the longest step a clock can take")
{
	// The largest step StepTimer accepts is a whole second of ticks, and the
	// most a clock can hold toward the next is one tick short of it. That is
	// the quotient closest to one, and it must not round up to it in float.
	const std::uint64_t longest = StepTimer::TicksPerSecond;
	const StepPosition last = step_position(3, longest - 1, longest);
	CHECK(last.fraction < 1.0f);
	CHECK(last.fraction > 0.99f);

	// And at the shortest, a step of one tick holds nothing toward the next.
	CHECK(step_position(3, 0, 1).fraction == 0.0f);
}

TEST_CASE("step_position answers 0 rather than leave the contract")
{
	CHECK(step_position(2, 5, 0).fraction == 0.0f);
	CHECK(step_position(2, 100, 100).fraction == 0.0f);
	CHECK(step_position(2, 250, 100).fraction == 0.0f);
	CHECK(step_position(2, 250, 100).step == 2);
}

TEST_CASE("a fresh StepTimer holds nothing toward its first step")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	timer.SetTargetElapsedTicks(1000);
	CHECK(timer.GetLeftOverTicks() == 0);
	CHECK(timer.GetTargetElapsedTicks() == 1000);

	const StepPosition position = step_position(timer.GetFrameCount(),
		timer.GetLeftOverTicks(), timer.GetTargetElapsedTicks());
	CHECK(position.step == 0);
	CHECK(position.fraction == 0.0f);
}

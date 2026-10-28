#include <doctest/doctest.h>

#include "engine/core/step_position.h"
#include "engine/core/step_timer.h"

#include <array>
#include <cstdint>
#include <limits>

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

TEST_CASE("step_position stays below one for a one-second Application period")
{
	// Application uses integer frames per second of at least one, so its
	// longest configured period is one second. StepTimer also accepts larger
	// periods; this is the ordinary Application boundary, not the timer's.
	const std::uint64_t period = StepTimer::TicksPerSecond;
	const StepPosition last = step_position(3, period - 1, period);
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

TEST_CASE("large valid step periods keep the rounded fraction below one")
{
	const std::uint64_t maximum = (std::numeric_limits<std::uint64_t>::max)();
	const std::uint64_t double_boundary = std::uint64_t{ 1 } << 53;
	const std::array<std::uint64_t, 5> periods = {
		std::uint64_t{ 1 } << 25,
		double_boundary - 1,
		double_boundary,
		double_boundary + 1,
		maximum
	};
	for (const std::uint64_t period : periods)
	{
		CAPTURE(period);
		const StepPosition position = step_position(maximum, period - 1, period);

		CHECK(position.step == maximum);
		CHECK(position.fraction >= 0.0f);
		CHECK(position.fraction < 1.0f);
		// The largest binary32 value below one, independent of the clamp's
		// choice of library operation.
		CHECK(position.fraction == 0x1.fffffep-1f);
	}
}

TEST_CASE("large step periods retain ordinary fractions and invalid-input zeros")
{
	const std::uint64_t period = std::uint64_t{ 1 } << 54;
	const std::uint64_t maximum = (std::numeric_limits<std::uint64_t>::max)();
	CHECK(step_position(1, period / 4, period).fraction == 0.25f);
	CHECK(step_position(1, period / 2, period).fraction == 0.5f);
	CHECK(step_position(1, 3 * (period / 4), period).fraction == 0.75f);
	CHECK(step_position(1, maximum / 2, maximum).fraction == 0.5f);
	CHECK(step_position(1, 0, maximum).fraction == 0.0f);

	const StepPosition tiny = step_position(7, 1, maximum);
	CHECK(tiny.step == 7);
	CHECK(tiny.fraction > 0.0f);
	CHECK(tiny.fraction < 1.0f);

	const StepPosition at_end = step_position(maximum, maximum, maximum);
	CHECK(at_end.step == maximum);
	CHECK(at_end.fraction == 0.0f);
	const StepPosition beyond_end = step_position(9, maximum, maximum - 1);
	CHECK(beyond_end.step == 9);
	CHECK(beyond_end.fraction == 0.0f);
	const StepPosition zero_period = step_position(11, maximum, 0);
	CHECK(zero_period.step == 11);
	CHECK(zero_period.fraction == 0.0f);
}

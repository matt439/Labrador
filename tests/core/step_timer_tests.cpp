#include <doctest/doctest.h>

#include "engine/core/step_timer.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <ratio>
#include <stdexcept>

// What Application's fixed and variable steps are made of, with the clock
// taken out.
//
// StepTimer::Tick reads the clock and hands the interval to Advance; these
// cases call Advance with intervals they chose, so every tick count below is
// exact. One case at the end runs Tick on the real clock.

using namespace labrador;

namespace
{
	static_assert(std::ratio_equal<StepTimer::Clock::period, std::nano>::value,
		"These counts assume a nanosecond clock, as MSVC's and libc++'s steady_clock are.");

	StepTimer::Clock::duration ns(std::int64_t count)
	{
		return std::chrono::duration_cast<StepTimer::Clock::duration>(std::chrono::nanoseconds(count));
	}

	constexpr std::uint64_t sixty_hertz = StepTimer::TicksPerSecond / 60;

	// Advances `timer` by `elapsed` and answers how many updates ran.
	int advance(StepTimer& timer, StepTimer::Clock::duration elapsed)
	{
		int updates = 0;
		timer.Advance(elapsed, [&]() { ++updates; });
		return updates;
	}
}

TEST_CASE("a fixed step snaps a 59.94 Hz interval to exactly one 60 Hz step")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	REQUIRE(timer.GetTargetElapsedTicks() == sixty_hertz);

	// 1 / 59.94 s is 16,683,350 ns, or 166,833 ticks: 167 over the step.
	for (int frame = 0; frame < 600; ++frame)
	{
		REQUIRE(advance(timer, ns(16683350)) == 1);
		REQUIRE(timer.GetLeftOverTicks() == 0);
	}
	CHECK(timer.GetElapsedTicks() == sixty_hertz);
	CHECK(timer.GetFrameCount() == 600);
	CHECK(timer.GetTotalTicks() == 600 * sixty_hertz);
}

TEST_CASE("the snap reaches a quarter of a millisecond either side of the step and no further")
{
	SUBCASE("2,499 ticks over snaps")
	{
		StepTimer timer;
		timer.SetFixedTimeStep(true);
		CHECK(advance(timer, ns(16916500)) == 1);
		CHECK(timer.GetLeftOverTicks() == 0);
	}
	SUBCASE("2,500 ticks over is kept")
	{
		StepTimer timer;
		timer.SetFixedTimeStep(true);
		CHECK(advance(timer, ns(16916600)) == 1);
		CHECK(timer.GetLeftOverTicks() == 2500);
	}
	SUBCASE("2,499 ticks under snaps")
	{
		StepTimer timer;
		timer.SetFixedTimeStep(true);
		CHECK(advance(timer, ns(16416700)) == 1);
		CHECK(timer.GetLeftOverTicks() == 0);
	}
	SUBCASE("2,500 ticks under is kept and runs no step")
	{
		StepTimer timer;
		timer.SetFixedTimeStep(true);
		CHECK(advance(timer, ns(16416600)) == 0);
		CHECK(timer.GetLeftOverTicks() == sixty_hertz - 2500);
	}
}

TEST_CASE("a one-second stall is clamped to a tenth of a second: six 60 Hz updates")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);

	std::uint64_t elapsed_seen = 0;
	int updates = 0;
	timer.Advance(ns(1000000000), [&]()
	{
		elapsed_seen = timer.GetElapsedTicks();
		++updates;
	});

	CHECK(updates == 6);
	CHECK(elapsed_seen == sixty_hertz);
	CHECK(timer.GetFrameCount() == 6);
	CHECK(timer.GetTotalTicks() == 6 * sixty_hertz);
	CHECK(timer.GetLeftOverTicks() == StepTimer::TicksPerSecond / 10 - 6 * sixty_hertz);
	// The stall's whole second still counts toward the frame rate, as one frame.
	CHECK(timer.GetFramesPerSecond() == 1);
}

TEST_CASE("a fixed step carries the remainder of a two-and-a-half step interval")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	timer.SetTargetElapsedTicks(100000);

	CHECK(advance(timer, ns(25000000)) == 2);
	CHECK(timer.GetLeftOverTicks() == 50000);
	CHECK(timer.GetTotalTicks() == 200000);

	CHECK(advance(timer, ns(5000000)) == 1);
	CHECK(timer.GetLeftOverTicks() == 0);
	CHECK(timer.GetTotalTicks() == 300000);
	CHECK(timer.GetFrameCount() == 3);
}

TEST_CASE("a variable step runs one update per advance with the clamped interval")
{
	StepTimer timer;
	REQUIRE(advance(timer, ns(5000000)) == 1);
	CHECK(timer.GetElapsedTicks() == 50000);
	CHECK(timer.GetTotalTicks() == 50000);

	REQUIRE(advance(timer, ns(1000000000)) == 1);
	CHECK(timer.GetElapsedTicks() == StepTimer::TicksPerSecond / 10);

	REQUIRE(advance(timer, ns(0)) == 1);
	CHECK(timer.GetElapsedTicks() == 0);

	REQUIRE(advance(timer, ns(-5000000)) == 1);
	CHECK(timer.GetElapsedTicks() == 0);

	CHECK(timer.GetFrameCount() == 4);
	CHECK(timer.GetTotalTicks() == 50000 + StepTimer::TicksPerSecond / 10);
	CHECK(timer.GetLeftOverTicks() == 0);
}

TEST_CASE("the frame rate is the frames counted in each second of the clock")
{
	StepTimer timer;
	for (int frame = 0; frame < 9; ++frame)
	{
		advance(timer, ns(100000000));
	}
	CHECK(timer.GetFramesPerSecond() == 0);
	advance(timer, ns(100000000));
	CHECK(timer.GetFramesPerSecond() == 10);
}

TEST_CASE("ResetElapsedTime drops the carried remainder and the frame rate")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	timer.SetTargetElapsedTicks(100000);
	for (int frame = 0; frame < 10; ++frame)
	{
		REQUIRE(advance(timer, ns(100000000)) == 10);
	}
	REQUIRE(timer.GetFramesPerSecond() == 10);
	REQUIRE(advance(timer, ns(25000000)) == 2);
	REQUIRE(timer.GetLeftOverTicks() == 50000);

	timer.ResetElapsedTime();
	CHECK(timer.GetLeftOverTicks() == 0);
	CHECK(timer.GetFramesPerSecond() == 0);

	CHECK(advance(timer, ns(5000000)) == 0);
	CHECK(timer.GetLeftOverTicks() == 50000);
	CHECK(timer.GetFrameCount() == 102);
}

TEST_CASE("the snap compare holds for a target near the top of the tick range")
{
	// An interval 2^63 ticks from the target: the difference no signed
	// 64-bit value can negate.
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	const std::uint64_t interval = StepTimer::TicksPerSecond / 10;
	timer.SetTargetElapsedTicks((std::uint64_t(1) << 63) + interval);

	CHECK(advance(timer, ns(100000000)) == 0);
	CHECK(timer.GetLeftOverTicks() == interval);

	timer.SetTargetElapsedTicks((std::numeric_limits<std::uint64_t>::max)());
	CHECK(advance(timer, ns(100000000)) == 0);
	CHECK(timer.GetLeftOverTicks() == 2 * interval);
}

TEST_CASE("a fixed period must advance time and leaves the old one on refusal")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	CHECK_THROWS_AS(timer.SetTargetElapsedTicks(0), std::invalid_argument);
	CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(-1.0), std::invalid_argument);
	CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(std::numeric_limits<double>::infinity()), std::invalid_argument);
	CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
	CHECK(timer.GetTargetElapsedTicks() == sixty_hertz);
}

TEST_CASE("Tick advances by the real clock")
{
	StepTimer timer;
	timer.SetFixedTimeStep(true);
	timer.SetTargetElapsedTicks(1);
	int callbacks = 0;
	struct Stop {};
	try
	{
		for (int tick = 0; tick < 1000 && callbacks == 0; ++tick)
		{
			timer.Tick([&]()
			{
				CHECK(timer.GetElapsedTicks() == 1);
				++callbacks;
				throw Stop{};
			});
		}
	}
	catch (const Stop&) {}
	CHECK(callbacks == 1);
}

#include "bench/frame_pacer.h"

#include <doctest/doctest.h>

#include <chrono>

TEST_CASE("the frame pacer never starts before its deadline")
{
	using Clock = std::chrono::steady_clock;
	const bench::FramePacer pacer;
	const Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(2);
	const Clock::time_point started = pacer.wait_until(deadline);
	CHECK(started >= deadline);
	CHECK(started <= Clock::now());

	// An overdue deadline must also work, including after using the timer.
	const Clock::time_point catch_up = pacer.wait_until(deadline);
	CHECK(catch_up >= started);
	CHECK(catch_up <= Clock::now());
}

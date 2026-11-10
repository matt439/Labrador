#include <doctest/doctest.h>

#include "engine/app/frame_timer.h"
#include "engine/app/application.h"

#include <chrono>
#include <cstdint>
#include <limits>

// What Application::frame_timings() publishes, with the clock taken out.
//
// FrameTimer reads no clock: Application hands it the time points it took,
// and this file hands it time points it chose, so every duration below is
// exact. The loop that takes the real ones is held by
// application_loop_tests.cpp, in the configurations that can run it.

namespace
{
	using labrador::FrameMarks;
	using labrador::FrameTimer;
	using labrador::FrameTimings;
	using std::chrono::milliseconds;
	using std::chrono::nanoseconds;
	using TimePoint = std::chrono::steady_clock::time_point;

	TimePoint at(int ms)
	{
		return TimePoint(milliseconds(ms));
	}

	// One rendered tick starting at `start`: updates for 4 ms, then 1 ms of
	// begin_frame, 3 of draw, 2 of submit and 5 of end_frame.
	FrameMarks marks_from(int start, int updates)
	{
		FrameMarks marks;
		marks.updates = updates;
		marks.updated = at(start + 4);
		marks.frame_begun = at(start + 5);
		marks.drawn = at(start + 8);
		marks.submitted = at(start + 10);
		marks.frame_ended = at(start + 15);
		return marks;
	}

	void run_tick(FrameTimer& timer, FrameTimings& timings, int start,
		int updates)
	{
		timer.begin(at(start));
		timer.publish(marks_from(start, updates), timings);
	}
}

TEST_CASE("frame_timings is all zero until the first frame is published")
{
	const FrameTimings timings;
	CHECK(timings.frame == 0);
	CHECK(timings.updates == 0);
	CHECK(timings.interval == nanoseconds::zero());
	CHECK(timings.update_time == nanoseconds::zero());
	CHECK(timings.begin_frame_time == nanoseconds::zero());
	CHECK(timings.draw_time == nanoseconds::zero());
	CHECK(timings.submit_time == nanoseconds::zero());
	CHECK(timings.end_frame_time == nanoseconds::zero());
}

TEST_CASE("each phase is the bracket around it, and frame counts published frames")
{
	FrameTimer timer;
	FrameTimings timings;

	run_tick(timer, timings, 100, 1);
	CHECK(timings.frame == 1);
	CHECK(timings.updates == 1);
	CHECK(timings.update_time == milliseconds(4));
	CHECK(timings.begin_frame_time == milliseconds(1));
	CHECK(timings.draw_time == milliseconds(3));
	CHECK(timings.submit_time == milliseconds(2));
	CHECK(timings.end_frame_time == milliseconds(5));

	run_tick(timer, timings, 120, 1);
	CHECK(timings.frame == 2);
}

TEST_CASE("the interval is zero on the first frame and the gap between tick starts after it")
{
	FrameTimer timer;
	FrameTimings timings;

	run_tick(timer, timings, 100, 1);
	CHECK(timings.interval == nanoseconds::zero());

	run_tick(timer, timings, 117, 1);
	CHECK(timings.interval == milliseconds(17));

	run_tick(timer, timings, 150, 1);
	CHECK(timings.interval == milliseconds(33));
}

TEST_CASE("a tick with no update, and one that catches up, say so")
{
	FrameTimer timer;
	FrameTimings timings;

	run_tick(timer, timings, 100, 0);
	CHECK(timings.updates == 0);

	run_tick(timer, timings, 150, 3);
	CHECK(timings.updates == 3);
}

TEST_CASE("a frame that is begun and never published leaves the snapshot as it was")
{
	FrameTimer timer;
	FrameTimings timings;

	run_tick(timer, timings, 100, 1);
	const FrameTimings before = timings;

	// A tick that threw, or rendered nothing, never reaches publish().
	timer.begin(at(117));

	CHECK(timings.frame == before.frame);
	CHECK(timings.update_time == before.update_time);
	CHECK(timings.interval == before.interval);

	// And the next frame that is published measures from the last one that
	// was, which is the frame before it on the record.
	run_tick(timer, timings, 133, 1);
	CHECK(timings.frame == 2);
	CHECK(timings.interval == milliseconds(33));
}

TEST_CASE("a discontinuity between ticks zeroes the next interval")
{
	FrameTimer timer;
	FrameTimings timings;

	run_tick(timer, timings, 100, 1);
	run_tick(timer, timings, 117, 1);
	REQUIRE(timings.interval == milliseconds(17));

	// Coming back from being hidden: nothing since the last tick was play.
	timer.discontinuity();
	run_tick(timer, timings, 5000, 1);
	CHECK(timings.interval == nanoseconds::zero());

	run_tick(timer, timings, 5017, 1);
	CHECK(timings.interval == milliseconds(17));
}

TEST_CASE("a discontinuity during a tick zeroes that frame's interval and the next")
{
	FrameTimer timer;
	FrameTimings timings;

	run_tick(timer, timings, 100, 1);

	// reset_elapsed_time() from inside an update, after a level load: the
	// tick began before the load, so its start is no baseline for the next.
	timer.begin(at(117));
	timer.discontinuity();
	timer.publish(marks_from(117, 1), timings);
	CHECK(timings.interval == nanoseconds::zero());

	run_tick(timer, timings, 3000, 1);
	CHECK(timings.interval == nanoseconds::zero());

	run_tick(timer, timings, 3017, 1);
	CHECK(timings.interval == milliseconds(17));
}

// The sleep of a minimised window's loop (WindowNotify::tick_minimized),
// with the clock taken out the same way: the ticks are StepTimer's, ten
// million to the second.

TEST_CASE("a minimised loop waits for what is left of the step, and no longer")
{
	// A 60 Hz step is 166,666 ticks.
	const std::uint64_t step = labrador::StepTimer::TicksPerSecond / 60;

	// Nothing held: the whole step, to the tick.
	CHECK(labrador::minimized_wait(0, step) == nanoseconds(16666600));

	// 10 ms held: what is left, 6.6666 ms.
	CHECK(labrador::minimized_wait(100000, step) == nanoseconds(6666600));

	// One tick short of the step is one tick, which is 100 ns.
	CHECK(labrador::minimized_wait(step - 1, step) == nanoseconds(100));
}

TEST_CASE("a minimised loop never waits past half the time the clock forgives")
{
	CHECK(labrador::longest_step_wait == milliseconds(50));

	// Exactly the cap, and one tick either side of it.
	const std::uint64_t cap_ticks = 500000;
	CHECK(labrador::minimized_wait(0, cap_ticks - 1) == nanoseconds(49999900));
	CHECK(labrador::minimized_wait(0, cap_ticks) == milliseconds(50));
	CHECK(labrador::minimized_wait(0, cap_ticks + 1) == milliseconds(50));

	// A 2 Hz step with most of it still to come, and a step no clock could
	// run: the cap, with no overflow on the way to it.
	const std::uint64_t slow = labrador::StepTimer::TicksPerSecond / 2;
	CHECK(labrador::minimized_wait(1000, slow) == milliseconds(50));
	CHECK(labrador::minimized_wait(0,
		(std::numeric_limits<std::uint64_t>::max)()) == milliseconds(50));

	// The same slow step with 10 ms left is 10 ms: the cap is a ceiling,
	// not the answer.
	CHECK(labrador::minimized_wait(slow - 100000, slow) == milliseconds(10));
}

TEST_CASE("a step already owed, or no step at all, is no wait")
{
	const std::uint64_t step = labrador::StepTimer::TicksPerSecond / 60;
	CHECK(labrador::minimized_wait(step, step) == nanoseconds::zero());
	CHECK(labrador::minimized_wait(step + 1, step) == nanoseconds::zero());
	CHECK(labrador::minimized_wait(0, 0) == nanoseconds::zero());
	CHECK(labrador::minimized_wait(5, 0) == nanoseconds::zero());
}

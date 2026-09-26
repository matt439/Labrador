#include <doctest/doctest.h>

#include "engine/app/application.h"

#include <stdexcept>
#include <limits>
#include <string>

using namespace labrador;

// The options are the one engine input a game reads out of a file it does not
// control, so every field that reaches an arithmetic operation is checked
// before a window exists (T6). These pin the checks; the Application
// constructor calls validate() as its first statement, so a throw here is a
// message at startup rather than a divide by zero on the first frame.

TEST_CASE("the defaults are valid")
{
	const ApplicationOptions options;
	CHECK_NOTHROW(options.validate());
}

TEST_CASE("the thread ceiling defaults to what the machine reports")
{
	// The whole point of the split: this number is a property of the MACHINE,
	// and not the renderer's view capacity or the partition count. One literal
	// serving all three is what this pins apart.
	CHECK(default_thread_count() >= 1);

	const ApplicationOptions options;
	CHECK(options.max_threads == default_thread_count());
	CHECK(options.max_threads >= options.min_threads);
}

TEST_CASE("the view capacity is a layout number, and independent of the pool")
{
	// Four-player split-screen is the widest layout either client has, and it
	// does not move when the machine changes. A build on a 32-thread part and a
	// build on a 2-thread part size their per-view recording state identically.
	const ApplicationOptions options;
	CHECK(options.view_capacity == 4);
}

TEST_CASE("a view capacity below one is rejected, naming the field")
{
	ApplicationOptions options;

	options.view_capacity = 0;
	CHECK_THROWS_AS(options.validate(), std::invalid_argument);

	options.view_capacity = -1;
	CHECK_THROWS_AS(options.validate(), std::invalid_argument);

	// Named, not merely rejected: Renderer::create_device throws on the same
	// value, and the difference between the two messages is whether the reader
	// is told which of their own fields to go and look at.
	try
	{
		options.validate();
		FAIL("validate() accepted a view capacity of -1");
	}
	catch (const std::invalid_argument& error)
	{
		const std::string message = error.what();
		CHECK(message.find("view_capacity") != std::string::npos);
	}
}

TEST_CASE("the checks that were already here still hold")
{
	// Regression cover for the fields validate() was carrying before
	// view_capacity joined them - each of these reaches a divisor.
	{
		ApplicationOptions options;
		options.target_fps = 0;
		CHECK_THROWS_AS(options.validate(), std::invalid_argument);
	}
	{
		ApplicationOptions options;
		options.min_threads = 0;
		CHECK_THROWS_AS(options.validate(), std::invalid_argument);
	}
	{
		ApplicationOptions options;
		options.min_threads = 4;
		options.max_threads = 2;
		CHECK_THROWS_AS(options.validate(), std::invalid_argument);
	}
	{
		ApplicationOptions options;
		options.min_window_width = 0;
		CHECK_THROWS_AS(options.validate(), std::invalid_argument);
	}
	{
		ApplicationOptions options;
		options.window_class_name.clear();
		CHECK_THROWS_AS(options.validate(), std::invalid_argument);
	}
}


TEST_CASE("accepted frame rates always produce an advancing timer period")
{
    ApplicationOptions options;
    options.target_fps = static_cast<int>(StepTimer::TicksPerSecond);
    CHECK_NOTHROW(options.validate());
    CHECK(StepTimer::SecondsToTicks(1.0 / options.target_fps) == 1);
    ++options.target_fps;
    CHECK_THROWS_AS(options.validate(), std::invalid_argument);
    StepTimer timer;
    timer.SetFixedTimeStep(true);
    CHECK_THROWS_AS(timer.SetTargetElapsedTicks(0), std::invalid_argument);
    CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(1.0 / options.target_fps), std::invalid_argument);
    CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(-1.0), std::invalid_argument);
    CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(std::numeric_limits<double>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS(timer.SetTargetElapsedSeconds(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
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

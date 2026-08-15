#include <doctest/doctest.h>

#include "engine/app/application.h"

#include <stdexcept>
#include <string>

using namespace artattack;

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
	// The whole point of the split: this number is a property of the MACHINE.
	// It used to be a literal 16, which also doubled as the renderer's view
	// capacity and as the partition count.
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

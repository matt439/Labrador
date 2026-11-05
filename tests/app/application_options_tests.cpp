#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/app/window.h"
#include "engine/core/step_timer.h"
#include "engine/math/vector2i.h"

#include <limits>
#include <stdexcept>
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
	// This number is a property of the MACHINE, and not the renderer's view
	// capacity or the partition count: no one literal serves all three.
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

TEST_CASE("a zero frame rate, inverted thread range, zero window minimum or empty class name is rejected")
{
	// Each of these would otherwise fail after startup: at a divisor, in an
	// inverted thread range or at a window class with no name.
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

TEST_CASE("the window an Application opens carries every window field of its options")
{
	ApplicationOptions options;
	options.window_class_name = L"WindowOptionsForClass";
	options.window_title = L"Window options for";
	options.client_size = Vector2I(900, 500);
	options.fullscreen = true;
	options.visible = false;
	options.cursor_visible = false;
	options.cursor_confined = true;
	options.min_window_width = 400;
	options.min_window_height = 300;

	// The size passed, not options.client_size: the caller decides it.
	const WindowOptions window = window_options_for(options, Vector2I(640, 360));
	CHECK(window.window_class_name == options.window_class_name);
	CHECK(window.window_title == options.window_title);
	CHECK(window.client_size == Vector2I(640, 360));
	CHECK(window.fullscreen);
	CHECK_FALSE(window.visible);
	CHECK_FALSE(window.cursor_visible);
	CHECK(window.cursor_confined);
	CHECK(window.min_window_width == 400);
	CHECK(window.min_window_height == 300);

	// And from the defaults, the window a game that sets nothing gets.
	const ApplicationOptions defaults;
	const WindowOptions plain = window_options_for(defaults, Vector2I(320, 200));
	CHECK(plain.window_class_name == defaults.window_class_name);
	CHECK(plain.window_title == defaults.window_title);
	CHECK(plain.fullscreen == defaults.fullscreen);
	CHECK(plain.visible == defaults.visible);
	CHECK(plain.cursor_visible == defaults.cursor_visible);
	CHECK(plain.cursor_confined == defaults.cursor_confined);
	CHECK(plain.min_window_width == defaults.min_window_width);
	CHECK(plain.min_window_height == defaults.min_window_height);
}

TEST_CASE("a client size is optional, and refused below the minimum window size by name")
{
	{
		const ApplicationOptions options;
		CHECK_FALSE(options.client_size.has_value());
	}
	{
		ApplicationOptions options;
		options.client_size = Vector2I(1366, 768);
		CHECK_NOTHROW(options.validate());
		options.client_size = Vector2I(options.min_window_width,
			options.min_window_height);
		CHECK_NOTHROW(options.validate());
	}

	const auto refused_naming = [](const ApplicationOptions& options,
		const char* field)
		{
			try
			{
				options.validate();
				FAIL("validate() accepted a client size below the minimum");
			}
			catch (const std::invalid_argument& error)
			{
				const std::string message = error.what();
				CHECK(message.find("client_size") != std::string::npos);
				CHECK(message.find(field) != std::string::npos);
			}
		};

	ApplicationOptions options;
	options.client_size = Vector2I(options.min_window_width - 1, 600);
	refused_naming(options, "min_window_width");
	options.client_size = Vector2I(800, options.min_window_height - 1);
	refused_naming(options, "min_window_height");
	options.client_size = Vector2I(0, 0);
	refused_naming(options, "min_window_width");
}

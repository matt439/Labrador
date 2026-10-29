#include <doctest/doctest.h>

#include "engine/math/rectanglei.h"
#include "engine/render/animation_strip.h"

#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
	using labrador::AnimationStrip;
	using labrador::RectangleI;
}

TEST_CASE("animation strips keep representable x after a wide positive product")
{
	const int minimum = (std::numeric_limits<int>::min)();
	const int width = 1073741824;
	const AnimationStrip strip(RectangleI(minimum, 7, width, 18),
		3, 0.375f, false);

	CHECK(strip.frame_count() == 3);
	CHECK(strip.frame_time() == 0.375f);
	CHECK_FALSE(strip.looping());
	CHECK(strip.frame_rect(0) == RectangleI(minimum, 7, width, 18));
	CHECK(strip.frame_rect(1) == RectangleI(-1073741824, 7, width, 18));
	CHECK(strip.frame_rect(2) == RectangleI(0, 7, width, 18));
}

TEST_CASE("animation strips keep representable x after a wide negative product")
{
	const int maximum = (std::numeric_limits<int>::max)();
	const int width = -1073741825;
	const AnimationStrip strip(RectangleI(maximum, -3, width, 5),
		3, 0.125f, true);

	CHECK(strip.frame_count() == 3);
	CHECK(strip.frame_time() == 0.125f);
	CHECK(strip.looping());
	CHECK(strip.frame_rect(0) == RectangleI(maximum, -3, width, 5));
	CHECK(strip.frame_rect(1) == RectangleI(1073741822, -3, width, 5));
	CHECK(strip.frame_rect(2) == RectangleI(-3, -3, width, 5));
}

TEST_CASE("animation strips preserve ordinary signed zero-width and boundary origins")
{
	struct Control
	{
		int first_x;
		int width;
		int count;
		std::array<int, 3> expected_x;
	};
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<Control, 6> controls = { {
		{ -4, 12, 3, { -4, 8, 20 } },
		{ 4, -3, 3, { 4, 1, -2 } },
		{ maximum, 0, 3, { maximum, maximum, maximum } },
		{ minimum, 0, 3, { minimum, minimum, minimum } },
		{ maximum - 1, 1, 2, { maximum - 1, maximum, 0 } },
		{ minimum + 1, -1, 2, { minimum + 1, minimum, 0 } }
	} };
	for (const Control& control : controls)
	{
		CAPTURE(control.first_x);
		CAPTURE(control.width);
		const AnimationStrip strip(RectangleI(control.first_x, 8, control.width, 32),
			control.count, 0.25f, true);
		CHECK(strip.frame_count() == control.count);
		CHECK(strip.frame_time() == 0.25f);
		CHECK(strip.looping());
		for (int index = 0; index < control.count; ++index)
		{
			CHECK(strip.frame_rect(index) == RectangleI(
				control.expected_x[static_cast<std::size_t>(index)],
				8, control.width, 32));
		}
	}
}

TEST_CASE("animation strips refuse an unrepresentable final frame x")
{
	struct Refusal
	{
		int first_x;
		int width;
		const char* coordinate;
	};
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<Refusal, 2> cases = { {
		{ maximum - 1, 1, "2147483648" },
		{ minimum + 1, -1, "-2147483649" }
	} };
	for (const Refusal& test : cases)
	{
		CAPTURE(test.first_x);
		CAPTURE(test.width);
		bool rejected = false;
		try
		{
			static_cast<void>(AnimationStrip(
				RectangleI(test.first_x, 7, test.width, 18), 3, 0.125f, false));
		}
		catch (const std::out_of_range& error)
		{
			rejected = true;
			const std::string message = error.what();
			CHECK(message.find("frame 2") != std::string::npos);
			CHECK(message.find("x coordinate") != std::string::npos);
			CHECK(message.find(test.coordinate) != std::string::npos);
			CHECK(message.find("int range") != std::string::npos);
		}
		CHECK(rejected);
	}
}

TEST_CASE("zero-count strips do not compute unused extreme coordinates")
{
	const int minimum = (std::numeric_limits<int>::min)();
	const int maximum = (std::numeric_limits<int>::max)();
	const std::array<RectangleI, 2> first_frames = { {
		RectangleI(maximum, 7, minimum, 18),
		RectangleI(minimum, 7, maximum, 18)
	} };
	for (const RectangleI& first_frame : first_frames)
	{
		const AnimationStrip strip(first_frame, 0, 0.5f, true);
		CHECK(strip.frame_count() == 0);
		CHECK(strip.frame_time() == 0.5f);
		CHECK(strip.looping());
		CHECK_THROWS_AS(strip.frame_rect(0), std::out_of_range);
		CHECK_THROWS_AS(strip.frame_rect(-1), std::out_of_range);
	}
}

TEST_CASE("animation strip cache queries reject indices outside the strip")
{
	const AnimationStrip strip(RectangleI(4, 8, 16, 32), 3, 0.25f, false);
	CHECK(strip.frame_rect(2) == RectangleI(36, 8, 16, 32));
	CHECK_THROWS_AS(strip.frame_rect(-1), std::out_of_range);
	CHECK_THROWS_AS(strip.frame_rect(3), std::out_of_range);
	CHECK_THROWS_AS(strip.frame_rect((std::numeric_limits<int>::max)()),
		std::out_of_range);
}

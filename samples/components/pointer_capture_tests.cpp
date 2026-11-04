#include <doctest/doctest.h>

#include "samples/components/pointer_capture.h"

#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"

#include <optional>
#include <vector>

using components::hit_target;
using components::PointerCapture;
using components::PointerPhase;
using components::PointerSample;
using components::PointerTarget;
using labrador::Mouse;
using labrador::MouseButton;
using labrador::RectangleF;
using labrador::Touch;
using labrador::Vector2F;
using labrador::Vector2I;

namespace
{
	// The two devices as the shell keeps them, fed by hand and polled once a
	// tick, with a capture reading them after each poll.
	struct Devices
	{
		Mouse mouse;
		Touch touch;
		PointerCapture capture;

		Devices()
		{
			this->mouse.set_focused(true);
			this->touch.set_focused(true);
			this->tick();
		}

		PointerSample tick()
		{
			this->mouse.poll();
			this->touch.poll();
			return this->capture.update(this->mouse, this->touch);
		}
	};

	void check(const PointerSample& sample, PointerPhase phase,
		const Vector2I& position = Vector2I())
	{
		CHECK(sample.phase == phase);
		if (phase != PointerPhase::cancelled && phase != PointerPhase::none)
		{
			CHECK(sample.position == position);
		}
	}
}

TEST_CASE("a click is pressed, held while it moves, and released where it lets go")
{
	Devices devices;
	devices.mouse.on_move(Vector2I(10, 20));
	devices.mouse.on_button_down(MouseButton::left);
	check(devices.tick(), PointerPhase::pressed, Vector2I(10, 20));
	CHECK(devices.capture.active());

	devices.mouse.on_move(Vector2I(40, 25));
	check(devices.tick(), PointerPhase::held, Vector2I(40, 25));

	devices.mouse.on_move(Vector2I(-5, 900));
	devices.mouse.on_button_up(MouseButton::left);
	check(devices.tick(), PointerPhase::released, Vector2I(-5, 900));
	CHECK_FALSE(devices.capture.active());

	check(devices.tick(), PointerPhase::none);
}

TEST_CASE("a tap is pressed, then released where the finger was last seen")
{
	Devices devices;
	devices.touch.on_down(7, Vector2I(100, 50));
	check(devices.tick(), PointerPhase::pressed, Vector2I(100, 50));

	devices.touch.on_move(7, Vector2I(130, 60));
	check(devices.tick(), PointerPhase::held, Vector2I(130, 60));

	devices.touch.on_up(7, Vector2I(130, 60));
	check(devices.tick(), PointerPhase::released, Vector2I(130, 60));
	check(devices.tick(), PointerPhase::none);
}

TEST_CASE("a tap over inside one tick still presses and then releases")
{
	Devices devices;
	devices.touch.on_down(7, Vector2I(5, 5));
	devices.touch.on_up(7, Vector2I(5, 5));
	check(devices.tick(), PointerPhase::pressed, Vector2I(5, 5));
	check(devices.tick(), PointerPhase::released, Vector2I(5, 5));
}

TEST_CASE("only the first finger is followed, and a second is ignored until it lifts")
{
	Devices devices;
	devices.touch.on_down(1, Vector2I(10, 10));
	check(devices.tick(), PointerPhase::pressed, Vector2I(10, 10));

	devices.touch.on_down(2, Vector2I(500, 500));
	check(devices.tick(), PointerPhase::held, Vector2I(10, 10));

	// The first lifts while the second stays: the second never began while
	// nothing was followed, so it is not followed now.
	devices.touch.on_up(1, Vector2I(10, 10));
	check(devices.tick(), PointerPhase::released, Vector2I(10, 10));
	devices.touch.on_move(2, Vector2I(510, 500));
	check(devices.tick(), PointerPhase::none);
	devices.touch.on_up(2, Vector2I(510, 500));
	check(devices.tick(), PointerPhase::none);

	// A new finger after that is followed.
	devices.touch.on_down(3, Vector2I(20, 20));
	check(devices.tick(), PointerPhase::pressed, Vector2I(20, 20));
}

TEST_CASE("the mouse is ignored while a finger is down and in the tick after it lifts")
{
	Devices devices;
	devices.touch.on_down(1, Vector2I(10, 10));
	check(devices.tick(), PointerPhase::pressed, Vector2I(10, 10));

	// The mouse a platform makes of the same touch, arriving alongside it.
	devices.mouse.on_move(Vector2I(10, 10));
	devices.mouse.on_button_down(MouseButton::left);
	check(devices.tick(), PointerPhase::held, Vector2I(10, 10));
	devices.mouse.on_button_up(MouseButton::left);
	devices.touch.on_up(1, Vector2I(10, 10));
	check(devices.tick(), PointerPhase::released, Vector2I(10, 10));

	// A compatibility click arriving just after the finger lifts.
	devices.mouse.on_button_down(MouseButton::left);
	check(devices.tick(), PointerPhase::none);
	devices.mouse.on_button_up(MouseButton::left);
	check(devices.tick(), PointerPhase::none);

	// With no finger near it, the mouse is the mouse again.
	devices.mouse.on_button_down(MouseButton::left);
	check(devices.tick(), PointerPhase::pressed, Vector2I(10, 10));
}

TEST_CASE("a finger is ignored while the mouse is followed")
{
	Devices devices;
	devices.mouse.on_move(Vector2I(1, 2));
	devices.mouse.on_button_down(MouseButton::left);
	check(devices.tick(), PointerPhase::pressed, Vector2I(1, 2));

	devices.touch.on_down(4, Vector2I(300, 300));
	check(devices.tick(), PointerPhase::held, Vector2I(1, 2));
	devices.mouse.on_button_up(MouseButton::left);
	check(devices.tick(), PointerPhase::released, Vector2I(1, 2));
	devices.touch.on_up(4, Vector2I(300, 300));
	check(devices.tick(), PointerPhase::none);
}

TEST_CASE("losing focus cancels the contact rather than releasing it")
{
	SUBCASE("the mouse")
	{
		Devices devices;
		devices.mouse.on_button_down(MouseButton::left);
		check(devices.tick(), PointerPhase::pressed);
		devices.mouse.set_focused(false);
		check(devices.tick(), PointerPhase::cancelled);
		CHECK_FALSE(devices.capture.active());

		// The button it was holding does not come back as a press.
		devices.mouse.set_focused(true);
		check(devices.tick(), PointerPhase::none);
	}
	SUBCASE("a finger")
	{
		Devices devices;
		devices.touch.on_down(1, Vector2I(10, 10));
		check(devices.tick(), PointerPhase::pressed, Vector2I(10, 10));
		devices.touch.set_focused(false);
		check(devices.tick(), PointerPhase::cancelled);
		devices.touch.set_focused(true);
		check(devices.tick(), PointerPhase::none);
	}
	SUBCASE("a touch the platform cancels")
	{
		Devices devices;
		devices.touch.on_down(1, Vector2I(10, 10));
		check(devices.tick(), PointerPhase::pressed, Vector2I(10, 10));
		devices.touch.on_cancel(1);
		check(devices.tick(), PointerPhase::cancelled);
	}
	SUBCASE("mouse buttons the window cancels")
	{
		Devices devices;
		devices.mouse.on_button_down(MouseButton::left);
		check(devices.tick(), PointerPhase::pressed);
		devices.mouse.cancel_buttons();
		check(devices.tick(), PointerPhase::cancelled);
	}
}

TEST_CASE("a page's cancel drops the contact, so its release does nothing")
{
	Devices devices;
	devices.mouse.on_button_down(MouseButton::left);
	check(devices.tick(), PointerPhase::pressed);
	devices.capture.cancel();
	CHECK_FALSE(devices.capture.active());
	devices.mouse.on_button_up(MouseButton::left);
	check(devices.tick(), PointerPhase::none);
}

TEST_CASE("a hit is the first enabled target, inside its left and top edges")
{
	const std::vector<PointerTarget> targets = {
		{ RectangleF(0.0f, 0.0f, 100.0f, 50.0f), true },
		{ RectangleF(0.0f, 50.0f, 100.0f, 50.0f), false },
		{ RectangleF(0.0f, 50.0f, 200.0f, 50.0f), true },
	};

	CHECK(hit_target(targets, Vector2F(0.0f, 0.0f)) == std::optional<size_t>(0));
	CHECK(hit_target(targets, Vector2F(99.5f, 49.5f)) == std::optional<size_t>(0));

	// The shared edge belongs to the row below, and the disabled row there
	// neither answers nor hides the enabled one under it.
	CHECK(hit_target(targets, Vector2F(10.0f, 50.0f)) == std::optional<size_t>(2));
	CHECK(hit_target(targets, Vector2F(150.0f, 60.0f)) == std::optional<size_t>(2));

	CHECK_FALSE(hit_target(targets, Vector2F(100.0f, 10.0f)).has_value());
	CHECK_FALSE(hit_target(targets, Vector2F(-0.5f, 10.0f)).has_value());
	CHECK_FALSE(hit_target(targets, Vector2F(10.0f, 100.0f)).has_value());
	CHECK_FALSE(hit_target({}, Vector2F(10.0f, 10.0f)).has_value());
}

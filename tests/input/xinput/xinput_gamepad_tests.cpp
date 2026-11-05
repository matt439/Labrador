#include <doctest/doctest.h>

#include "engine/input/xinput/xinput_gamepad.h"

#include <cstdint>

using namespace labrador;

// The XInput pad's translation, asserted with no pad. DirectXTK's state is
// filled here by hand, as standard_gamepad_tests.cpp fills the browser's.
// Compiled into the XInput build only, beside the file it tests.

namespace
{
	using State = DirectX::GamePad::State;

	// A connected pad at rest.
	State xinput_pad()
	{
		State pad{};
		pad.connected = true;
		return pad;
	}

	uint16_t bit(GamepadButton button)
	{
		return static_cast<uint16_t>(1u << static_cast<unsigned int>(button));
	}

	// Each of DirectXTK's fourteen buttons, with the engine button it must
	// become.
	struct ButtonCase
	{
		void (*press)(State&);
		GamepadButton button;
	};

	const ButtonCase button_cases[] = {
		{ [](State& pad) { pad.buttons.a = true; }, GamepadButton::a },
		{ [](State& pad) { pad.buttons.b = true; }, GamepadButton::b },
		{ [](State& pad) { pad.buttons.x = true; }, GamepadButton::x },
		{ [](State& pad) { pad.buttons.y = true; }, GamepadButton::y },
		{ [](State& pad) { pad.buttons.leftShoulder = true; },
			GamepadButton::left_shoulder },
		{ [](State& pad) { pad.buttons.rightShoulder = true; },
			GamepadButton::right_shoulder },
		{ [](State& pad) { pad.buttons.leftStick = true; },
			GamepadButton::left_stick },
		{ [](State& pad) { pad.buttons.rightStick = true; },
			GamepadButton::right_stick },
		{ [](State& pad) { pad.buttons.back = true; }, GamepadButton::back },
		{ [](State& pad) { pad.buttons.start = true; }, GamepadButton::start },
		{ [](State& pad) { pad.dpad.up = true; }, GamepadButton::dpad_up },
		{ [](State& pad) { pad.dpad.down = true; }, GamepadButton::dpad_down },
		{ [](State& pad) { pad.dpad.left = true; }, GamepadButton::dpad_left },
		{ [](State& pad) { pad.dpad.right = true; }, GamepadButton::dpad_right },
	};
}

TEST_CASE("each XInput button sets its own engine bit and no other")
{
	// One case per button, so a swapped pair - left and right on the pad, or
	// back and start - fails on both of its halves.
	for (const ButtonCase& one : button_cases)
	{
		State pad = xinput_pad();
		one.press(pad);
		const GamepadState state = gamepad_state_from_xinput(pad);
		CHECK(state.connected);
		CHECK(state.buttons == bit(one.button));
	}
}

TEST_CASE("every XInput button held at once is every engine bit")
{
	State pad = xinput_pad();
	uint16_t expected = 0;
	for (const ButtonCase& one : button_cases)
	{
		one.press(pad);
		expected = static_cast<uint16_t>(expected | bit(one.button));
	}

	CHECK(gamepad_state_from_xinput(pad).buttons == expected);
	CHECK(expected == 0x3FFF);
}

TEST_CASE("both sticks' y is negated to +y down and x is not")
{
	State pad = xinput_pad();
	pad.thumbSticks.leftX = -0.5f;
	pad.thumbSticks.leftY = 1.0f;
	pad.thumbSticks.rightX = 0.75f;
	pad.thumbSticks.rightY = -0.25f;

	const GamepadState state = gamepad_state_from_xinput(pad);
	CHECK(state.left_stick.x == doctest::Approx(-0.5f));
	CHECK(state.left_stick.y == doctest::Approx(-1.0f));
	CHECK(state.right_stick.x == doctest::Approx(0.75f));
	CHECK(state.right_stick.y == doctest::Approx(0.25f));
}

TEST_CASE("the triggers are analogue and are not buttons")
{
	State pad = xinput_pad();
	pad.triggers.left = 0.25f;
	pad.triggers.right = 1.0f;

	const GamepadState state = gamepad_state_from_xinput(pad);
	CHECK(state.left_trigger == doctest::Approx(0.25f));
	CHECK(state.right_trigger == doctest::Approx(1.0f));
	CHECK(state.buttons == 0);
}

TEST_CASE("a disconnected pad is neutral whatever DirectXTK left in it")
{
	State pad = xinput_pad();
	pad.connected = false;
	pad.buttons.a = true;
	pad.thumbSticks.leftY = 1.0f;
	pad.triggers.right = 1.0f;

	const GamepadState state = gamepad_state_from_xinput(pad);
	CHECK_FALSE(state.connected);
	CHECK(state.buttons == 0);
	CHECK(state.left_stick.y == doctest::Approx(0.0f));
	CHECK(state.right_trigger == doctest::Approx(0.0f));
}

TEST_CASE("the reader asks DirectXTK for no deadzone")
{
	// apply_deadzone is the only deadzone; a second one inside DirectXTK
	// would leave part of the stick's travel dead before it.
	CHECK(xinput_dead_zone == DirectX::GamePad::DEAD_ZONE_NONE);
}

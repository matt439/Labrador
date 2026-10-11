#include <doctest/doctest.h>

#include "engine/input/web/standard_gamepad.h"

#include <cstdint>
#include <cstring>
#include <vector>

using namespace labrador;

// The browser pad's translation and slot assignment, asserted with no
// browser. Node has no Gamepad API, so the event Emscripten would fill from
// navigator.getGamepads() is filled here by hand. Compiled into the browser
// build only, beside the file it tests, as dom_events_tests.cpp is.

namespace
{
	// A connected pad on the standard layout, at rest.
	EmscriptenGamepadEvent standard_pad(int index)
	{
		EmscriptenGamepadEvent pad;
		std::memset(&pad, 0, sizeof(pad));
		pad.connected = true;
		pad.index = index;
		pad.numButtons = 17;
		pad.numAxes = 4;
		std::strcpy(pad.mapping, "standard");
		std::strcpy(pad.id, "Test pad (STANDARD GAMEPAD)");
		return pad;
	}

	GamepadSlots empty_slots()
	{
		return { no_gamepad, no_gamepad, no_gamepad, no_gamepad };
	}

	GamepadSlots assigned(GamepadSlots slots, std::vector<int> present)
	{
		assign_gamepad_slots(slots, present);
		return slots;
	}
}

TEST_CASE("a standard pad's buttons are read by their W3C index")
{
	EmscriptenGamepadEvent pad = standard_pad(0);
	pad.digitalButton[0] = true;
	pad.digitalButton[9] = true;
	pad.digitalButton[14] = true;

	const GamepadState state = gamepad_state_from_standard(pad);
	CHECK(state.connected);
	CHECK(state.is_down(GamepadButton::a));
	CHECK(state.is_down(GamepadButton::start));
	CHECK(state.is_down(GamepadButton::dpad_left));
	CHECK_FALSE(state.is_down(GamepadButton::b));
	CHECK_FALSE(state.is_down(GamepadButton::dpad_right));
}

TEST_CASE("every named button has its own index")
{
	const GamepadButton by_index[] = {
		GamepadButton::a, GamepadButton::b, GamepadButton::x, GamepadButton::y,
		GamepadButton::left_shoulder, GamepadButton::right_shoulder,
	};
	for (int index = 0; index < 6; ++index)
	{
		EmscriptenGamepadEvent pad = standard_pad(0);
		pad.digitalButton[index] = true;
		const GamepadState state = gamepad_state_from_standard(pad);
		CHECK(state.buttons == static_cast<uint16_t>(
			1u << static_cast<unsigned int>(by_index[index])));
	}

	const GamepadButton from_eight[] = {
		GamepadButton::back, GamepadButton::start, GamepadButton::left_stick,
		GamepadButton::right_stick, GamepadButton::dpad_up,
		GamepadButton::dpad_down, GamepadButton::dpad_left,
		GamepadButton::dpad_right,
	};
	for (int offset = 0; offset < 8; ++offset)
	{
		EmscriptenGamepadEvent pad = standard_pad(0);
		pad.digitalButton[8 + offset] = true;
		const GamepadState state = gamepad_state_from_standard(pad);
		CHECK(state.buttons == static_cast<uint16_t>(
			1u << static_cast<unsigned int>(from_eight[offset])));
	}
}

TEST_CASE("the triggers are analogue and are not buttons")
{
	EmscriptenGamepadEvent pad = standard_pad(0);
	pad.digitalButton[6] = true;
	pad.analogButton[6] = 0.25;
	pad.digitalButton[7] = true;
	pad.analogButton[7] = 1.0;

	const GamepadState state = gamepad_state_from_standard(pad);
	CHECK(state.left_trigger == doctest::Approx(0.25f));
	CHECK(state.right_trigger == doctest::Approx(1.0f));
	CHECK(state.buttons == 0);
}

TEST_CASE("the sticks keep the standard layout's sign, which is already +y down")
{
	EmscriptenGamepadEvent pad = standard_pad(0);
	pad.axis[0] = -0.5;
	pad.axis[1] = 1.0;
	pad.axis[2] = 0.75;
	pad.axis[3] = -1.0;

	const GamepadState state = gamepad_state_from_standard(pad);
	CHECK(state.left_stick.x == doctest::Approx(-0.5f));
	CHECK(state.left_stick.y == doctest::Approx(1.0f));
	CHECK(state.right_stick.x == doctest::Approx(0.75f));
	CHECK(state.right_stick.y == doctest::Approx(-1.0f));
}

TEST_CASE("a pad without the standard mapping is reported absent")
{
	EmscriptenGamepadEvent pad = standard_pad(0);
	pad.digitalButton[0] = true;
	std::strcpy(pad.mapping, "");

	CHECK_FALSE(is_standard_gamepad(pad));
	const GamepadState state = gamepad_state_from_standard(pad);
	CHECK_FALSE(state.connected);
	CHECK(state.buttons == 0);
}

TEST_CASE("a standard pad short of the buttons or axes the engine reads is absent")
{
	EmscriptenGamepadEvent buttons = standard_pad(0);
	buttons.numButtons = 15;
	CHECK_FALSE(is_standard_gamepad(buttons));

	EmscriptenGamepadEvent axes = standard_pad(0);
	axes.numAxes = 2;
	CHECK_FALSE(is_standard_gamepad(axes));

	// The guide button is not read, so a pad without one still plays.
	EmscriptenGamepadEvent no_guide = standard_pad(0);
	no_guide.numButtons = 16;
	CHECK(is_standard_gamepad(no_guide));
}

TEST_CASE("a disconnected pad reads as an empty slot")
{
	EmscriptenGamepadEvent pad = standard_pad(0);
	pad.digitalButton[0] = true;
	pad.axis[0] = 1.0;
	pad.connected = false;

	const GamepadState state = gamepad_state_from_standard(pad);
	CHECK_FALSE(state.connected);
	CHECK(state.buttons == 0);
	CHECK(state.left_stick.x == 0.0f);
}

TEST_CASE("pads take the lowest free slots, whatever the browser numbered them")
{
	CHECK(assigned(empty_slots(), {}) == empty_slots());
	CHECK(assigned(empty_slots(), { 5 }) ==
		GamepadSlots{ 5, no_gamepad, no_gamepad, no_gamepad });
	CHECK(assigned(empty_slots(), { 0, 2, 7 }) ==
		GamepadSlots{ 0, 2, 7, no_gamepad });
}

TEST_CASE("a pad keeps its slot while it stays connected")
{
	GamepadSlots slots = assigned(empty_slots(), { 0, 1, 2 });

	// The first leaves. The others stay where they were rather than closing
	// the gap, so the player in slot 2 is still player 2.
	slots = assigned(slots, { 1, 2 });
	CHECK(slots == GamepadSlots{ no_gamepad, 1, 2, no_gamepad });

	// It comes back as 4, which takes the slot it left, being the lowest
	// free one, and the other two do not move.
	slots = assigned(slots, { 1, 2, 4 });
	CHECK(slots == GamepadSlots{ 4, 1, 2, no_gamepad });

	// A sample that changes nothing changes nothing.
	CHECK(assigned(slots, { 1, 2, 4 }) == slots);
}

TEST_CASE("a fifth pad waits until a slot is free")
{
	GamepadSlots slots = assigned(empty_slots(), { 0, 1, 2, 3, 4 });
	CHECK(slots == GamepadSlots{ 0, 1, 2, 3 });

	slots = assigned(slots, { 0, 2, 3, 4 });
	CHECK(slots == GamepadSlots{ 0, 4, 2, 3 });
}

TEST_CASE("every pad gone empties every slot")
{
	const GamepadSlots slots = assigned(empty_slots(), { 3, 6 });
	CHECK(assigned(slots, {}) == empty_slots());
}

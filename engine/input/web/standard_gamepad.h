#pragma once

#include "engine/input/gamepad.h"
#include "engine/input/gamepads.h"

#include <emscripten/html5.h>

#include <array>
#include <span>

namespace labrador
{
	// What the browser's pads mean to the engine: the half of
	// engine/input/web/gamepad_reader.cpp that needs no browser. It is out of
	// that file so that InputTests can pin it under Node, which has no Gamepad
	// API to hand a pad over from, by filling the event Emscripten fills.

	// Whether the browser mapped this pad onto the W3C "standard" layout, with
	// every button and axis the engine reads. A pad it could not map reports
	// its buttons and axes in whatever order its driver chose, so reading it
	// by index would put "a" wherever that happened to be. Such a pad is
	// reported absent rather than guessed at - it takes no slot, and a game
	// sees no pad at all.
	bool is_standard_gamepad(const EmscriptenGamepadEvent& pad);

	// The pad as the engine's GamepadState, or a disconnected state for a pad
	// that is not connected or not standard. No negation: the standard layout
	// is already +y down, which is this engine's convention.
	GamepadState gamepad_state_from_standard(const EmscriptenGamepadEvent& pad);

	// Which browser pad, by Gamepad.index, is in each of the engine's slots.
	inline constexpr int no_gamepad = -1;
	using GamepadSlots = std::array<int, Gamepads::max_count>;

	// Moves `slots` on by one sample. `present` is the index of every
	// connected standard pad, ascending.
	//
	// A PAD KEEPS ITS SLOT WHILE IT STAYS CONNECTED, which is the rule
	// XInput's user index keeps. A pad that has gone frees its slot, and a pad
	// seen for the first time takes the lowest free one, so a browser that
	// numbers a reconnected pad 5 still puts it in a slot a game reads. A
	// fifth pad waits, unseen, until one of the four leaves.
	//
	// The browser's index is the identity, because it is the one the Gamepad
	// API keeps for a pad's whole connection. A pad that leaves and another
	// that arrives at the same index between two samples are one occupant;
	// gamepad.h says why the engine does not try to tell them apart.
	void assign_gamepad_slots(GamepadSlots& slots, std::span<const int> present);
}

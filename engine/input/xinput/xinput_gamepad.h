#pragma once

#include "engine/input/gamepad.h"

#include <GamePad.h>

namespace labrador
{
	// What an XInput pad means to the engine: the half of
	// engine/input/xinput/gamepad_reader.cpp that needs no pad. It is out of
	// that file so that InputTests can pin it by filling the state DirectXTK
	// fills, as standard_gamepad.h does for the browser.

	// DEAD_ZONE_NONE, deliberately. The deadzone is applied once, by whoever
	// knows what the stick is being used for, with apply_deadzone. Asking for
	// one here and letting a caller apply a second by hand is exactly the
	// defect this module was written to remove - and the two copies it
	// replaces asked for *different* ones, so the same physical stick position
	// meant two things depending on which was reading.
	inline constexpr DirectX::GamePad::DeadZone xinput_dead_zone =
		DirectX::GamePad::DEAD_ZONE_NONE;

	// The pad as the engine's GamepadState, or a disconnected state for a pad
	// that is not connected. Both sticks' y is negated here, the one place:
	// XInput reports +y up, and this engine is +y down everywhere else.
	GamepadState gamepad_state_from_xinput(const DirectX::GamePad::State& pad);
}

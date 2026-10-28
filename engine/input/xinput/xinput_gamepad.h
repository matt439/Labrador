#pragma once

#include "engine/input/gamepad.h"

#include <GamePad.h>

namespace labrador
{
	// Converts a supplied DirectXTK snapshot without polling a device, so
	// InputTests can check translation with hand-written states.

	// Read unfiltered sticks. The caller chooses one deadzone for its use with
	// apply_deadzone; filtering here too would apply that policy twice (T1).
	inline constexpr DirectX::GamePad::DeadZone xinput_dead_zone =
		DirectX::GamePad::DEAD_ZONE_NONE;

	// A disconnected pad returns a neutral state. Connected snapshots preserve
	// buttons and trigger levels and negate both sticks' y coordinates, mapping
	// XInput's +y up to the engine's +y down convention.
	GamepadState gamepad_state_from_xinput(const DirectX::GamePad::State& pad);
}

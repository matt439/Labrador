#pragma once

#include "engine/input/keyboard.h"
#include "engine/math/vector2f.h"
#include "samples/local_multiplayer/arena.h"

#include <array>

namespace multiplayer
{
	// Stick i belongs to player i. Keyboard input remains available when a
	// controller is absent; callers pass the neutral state Gamepads reports.
	std::array<mattmath::Vector2F, Arena::player_count> read_directions(
		const labrador::Keyboard& keyboard,
		const std::array<mattmath::Vector2F, Arena::player_count>& sticks);
}

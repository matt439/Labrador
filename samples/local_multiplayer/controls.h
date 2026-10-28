#pragma once

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/math/vector2f.h"
#include "samples/local_multiplayer/arena.h"
#include "samples/local_multiplayer/join_rules.h"

#include <array>
#include <string>

namespace multiplayer
{
	// Stick i belongs to player i. Keyboard input remains available when a
	// controller is absent; callers pass the neutral state Gamepads reports.
	std::array<labrador::Vector2F, Arena::player_count> read_directions(
		const labrador::Keyboard& keyboard,
		const std::array<labrador::Vector2F, Arena::player_count>& sticks);

	// A joined player reads only its assigned source. Pad indices are backend
	// slots, independent of player indices; absent pads contribute neutral sticks.
	std::array<labrador::Vector2F, Arena::player_count> read_bound_directions(
		const labrador::Keyboard& keyboard,
		const std::array<labrador::Vector2F, labrador::Gamepads::max_count>& sticks,
		const std::array<InputSource, Arena::player_count>& sources);

	std::wstring source_name(InputSource source);
}

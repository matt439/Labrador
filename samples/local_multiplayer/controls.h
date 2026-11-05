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
	// The two keyboard players' keys, each in one place: the four that move,
	// the one that joins on the menu and selects a row, and the one that
	// leaves. Positions, not characters (engine/input/keyboard.h).
	struct KeyBindings
	{
		labrador::Key left;
		labrador::Key right;
		labrador::Key up;
		labrador::Key down;
		labrador::Key confirm;
		labrador::Key leave;
	};

	// Indexed by source_index: InputSource::keyboard_left's, then
	// keyboard_right's, which are also players 1 and 2 in read_directions.
	inline constexpr std::array<KeyBindings, 2> keyboard_bindings = {{
		{ labrador::Key::a, labrador::Key::d, labrador::Key::w, labrador::Key::s,
			labrador::Key::f, labrador::Key::g },
		{ labrador::Key::left, labrador::Key::right, labrador::Key::up, labrador::Key::down,
			labrador::Key::enter, labrador::Key::backspace },
	}};

	// The bindings of a keyboard source, or nullptr for a pad or none.
	const KeyBindings* key_bindings(InputSource source);

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

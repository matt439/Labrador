#include "samples/local_multiplayer/controls.h"

#include "engine/input/gamepad.h"

using namespace labrador;

namespace multiplayer
{
	std::array<Vector2F, Arena::player_count> read_directions(const Keyboard& keyboard,
		const std::array<Vector2F, Arena::player_count>& sticks)
	{
		const std::array<std::array<Key, 4>, Arena::player_count> bindings = {{
			{ Key::a, Key::d, Key::w, Key::s },
			{ Key::left, Key::right, Key::up, Key::down }
		}};
		std::array<Vector2F, Arena::player_count> directions;
		for (size_t player = 0; player < Arena::player_count; ++player)
		{
			Vector2F& direction = directions[player];
			direction = apply_deadzone(sticks[player], 0.2f);
			if (keyboard.held(bindings[player][0])) direction.x -= 1.0f;
			if (keyboard.held(bindings[player][1])) direction.x += 1.0f;
			if (keyboard.held(bindings[player][2])) direction.y -= 1.0f;
			if (keyboard.held(bindings[player][3])) direction.y += 1.0f;
		}
		return directions;
	}

	std::array<Vector2F, Arena::player_count> read_bound_directions(
		const Keyboard& keyboard,
		const std::array<Vector2F, Gamepads::max_count>& sticks,
		const std::array<InputSource, Arena::player_count>& sources)
	{
		const std::array<Vector2F, Arena::player_count> no_sticks = {};
		const std::array<Vector2F, Arena::player_count> keys =
			read_directions(keyboard, no_sticks);
		std::array<Vector2F, Arena::player_count> directions = {};
		for (std::size_t player = 0; player < Arena::player_count; ++player)
		{
			const InputSource source = sources[player];
			if (source == InputSource::keyboard_left)
			{
				directions[player] = keys[0];
			}
			else if (source == InputSource::keyboard_right)
			{
				directions[player] = keys[1];
			}
			else if (const int slot = pad_slot(source); slot >= 0)
			{
				directions[player] = apply_deadzone(
					sticks[static_cast<std::size_t>(slot)], 0.2f);
			}
		}
		return directions;
	}

	std::wstring source_name(InputSource source)
	{
		if (source == InputSource::keyboard_left) return L"WASD keyboard";
		if (source == InputSource::keyboard_right) return L"Arrow keyboard";
		const int slot = pad_slot(source);
		return slot >= 0 ? L"Pad slot " + std::to_wstring(slot) : L"Not joined";
	}
}

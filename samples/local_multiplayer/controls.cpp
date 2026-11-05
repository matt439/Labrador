#include "samples/local_multiplayer/controls.h"

#include "engine/input/gamepad.h"

using namespace labrador;

namespace multiplayer
{
	namespace
	{
		// How far a stick has to move before it counts, applied once by the
		// code that knows what the stick is for (engine/input/gamepad.h).
		constexpr float stick_deadzone = 0.2f;
	}

	static_assert(keyboard_bindings.size() == Arena::player_count);

	const KeyBindings* key_bindings(InputSource source)
	{
		if (source == InputSource::keyboard_left || source == InputSource::keyboard_right)
		{
			return &keyboard_bindings[source_index(source)];
		}
		return nullptr;
	}

	std::array<Vector2F, Arena::player_count> read_directions(const Keyboard& keyboard,
		const std::array<Vector2F, Arena::player_count>& sticks)
	{
		std::array<Vector2F, Arena::player_count> directions;
		for (size_t player = 0; player < Arena::player_count; ++player)
		{
			const KeyBindings& keys = keyboard_bindings[player];
			Vector2F& direction = directions[player];
			direction = apply_deadzone(sticks[player], stick_deadzone);
			if (keyboard.held(keys.left)) direction.x -= 1.0f;
			if (keyboard.held(keys.right)) direction.x += 1.0f;
			if (keyboard.held(keys.up)) direction.y -= 1.0f;
			if (keyboard.held(keys.down)) direction.y += 1.0f;
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
			if (key_bindings(source) != nullptr)
			{
				directions[player] = keys[source_index(source)];
			}
			else if (const int slot = pad_slot(source); slot >= 0)
			{
				directions[player] = apply_deadzone(
					sticks[static_cast<std::size_t>(slot)], stick_deadzone);
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

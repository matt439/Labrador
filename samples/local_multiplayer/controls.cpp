#include "samples/local_multiplayer/controls.h"

#include "engine/input/gamepad.h"

using namespace labrador;
using namespace mattmath;

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
}

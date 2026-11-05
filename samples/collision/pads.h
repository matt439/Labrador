#pragma once

#include "engine/math/vector2f.h"

#include <span>

namespace collision
{
	// One pad slot, as CollisionState reads it: the left stick with its
	// deadzone applied, and whether Y (reset) and B (quit) were pressed this
	// frame. An empty slot reads as a stick at rest with nothing pressed.
	struct PadReading
	{
		labrador::Vector2F stick = { 0.0f, 0.0f };
		bool reset_pressed = false;
		bool quit_pressed = false;
	};

	// What every slot said together.
	struct PadsInput
	{
		labrador::Vector2F stick = { 0.0f, 0.0f };
		bool reset_requested = false;
		bool quit_requested = false;
	};

	// A one-player game's pad policy, the template's (combine_pads in
	// samples/minimal/rules/motion.h): every slot counts, because the system
	// need not put the only pad in slot 0 - after another pad is unplugged it
	// can sit in slot 1 or 3 with slot 0 empty. The sticks are summed, and
	// Arena::step limits the direction to unit length; Y or B on any slot
	// resets or quits.
	PadsInput combine_pads(std::span<const PadReading> slots);
}

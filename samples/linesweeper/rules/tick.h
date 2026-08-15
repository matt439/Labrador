#pragma once

#include "samples/linesweeper/rules/world.h"

#include <cstdint>

// The verb. One function, one fixed step, and the only thing in this sample
// that changes a World.
//
// PRESENTATION MAY NOT INCLUDE THIS FILE. That is the layer rule, and this is
// the header it is drawn around: rules/world.h is the value and anybody may
// read it, rules/tables.h is the data behind it, and this is the step.
// states/ is the one place that sees both a keyboard and this declaration,
// and it is where a key becomes a button (README, Three layers).
namespace linesweeper
{
	// What a player can say, one bit each, packed into the byte the World
	// keeps as `input`.
	//
	// Constants and not an `enum class`, which is what CONVENTIONS asks for
	// everywhere else. The thing being built here is a mask, and an enum class
	// needs three operator overloads written before it can be OR-ed into one -
	// which is a type standing in front of a value to no end (T11). These are
	// std::uint8_t and World::input is a std::uint8_t, so
	// `button_left | button_soft_drop` is the byte the field wants with
	// nothing converting on the way.
	inline constexpr std::uint8_t button_none = 0;
	inline constexpr std::uint8_t button_left = 1 << 0;
	inline constexpr std::uint8_t button_right = 1 << 1;
	inline constexpr std::uint8_t button_soft_drop = 1 << 2;
	inline constexpr std::uint8_t button_hard_drop = 1 << 3;
	inline constexpr std::uint8_t button_rotate_clockwise = 1 << 4;
	inline constexpr std::uint8_t button_rotate_anticlockwise = 1 << 5;
	inline constexpr std::uint8_t button_hold = 1 << 6;

	// One fixed step of the match, and the whole of the game.
	//
	// THE INPUT IS A PARAMETER AND THE TIME IS NOT. There is no dt here and
	// there will not be one: every duration in these rules is counted in
	// ticks, so a match is a function of its starting World and the sequence
	// of bytes handed to this call - which is what makes a replay a
	// std::vector<std::uint8_t> and nothing else, and what makes it
	// bit-identical on a machine that ran the original at nine frames a
	// second. main.cpp pins the tick rate at sixty because these tables assume
	// it; nothing in the engine pins it for them.
	//
	// The order inside is fixed and worth knowing, because a player can feel
	// it: spawn if there is no piece, hold, rotate, shift, hard drop, gravity,
	// lock. Rotation before shift is what lets a piece kicked off a wall be
	// walked back in on the same tick; hard drop before gravity is what stops
	// a piece the player just slammed also being charged for the row gravity
	// was about to give it.
	//
	// A topped-out World ignores this entirely. Restarting is not a verb
	// either - it is `world = World{}` at the call site, which is the whole
	// argument of README's "The match is one value".
	void tick(World& world, std::uint8_t input);
}

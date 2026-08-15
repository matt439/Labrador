#pragma once

#include <cstdint>

namespace labrador
{
	// Which group an object belongs to: exactly one bit, set by the game.
	//
	// The engine assigns no meaning to any bit and never compares one to a
	// named constant - there are no named constants here, and that absence is
	// the point. What this replaces is a 22-value enum of game nouns
	// (player_team_a, projectile_mist_team_b, structure_ramp_left) reachable
	// from the collision interface, which is finding #13: the engine could not
	// be handed a second game without either editing the enum or lying to it.
	using CollisionLayer = std::uint32_t;

	// Which layers an object responds to. Zero means "nothing, right now" -
	// a legitimate and useful answer, and the one a dead player gives.
	using CollisionMask = std::uint32_t;

	// The game's own classification, carried through the engine untouched.
	//
	// Nothing in engine/ reads this. It exists so a response can recover what
	// it hit without the engine having to know the vocabulary: the game casts
	// its enum in on one side and out on the other. That is the whole of
	// "the engine decides whether things collide; the game decides what it
	// means" (PHILOSOPHY, Collision).
	using CollisionTag = std::uint32_t;

	// Two objects are a candidate pair when each one's layer is in the other's
	// mask.
	//
	// Both directions must agree, so either side can veto. That symmetry is
	// what the hand-written filters it replaces did not have: a player asked
	// only whether the other thing was a structure, a projectile asked whether
	// it was a structure or an enemy player, and the pair (player, projectile)
	// therefore passed one test and failed the other. Which response ran came
	// down to which object the loop happened to be iterating - see
	// find_contacts.
	constexpr bool layers_collide(CollisionLayer a_layer, CollisionMask a_mask,
		CollisionLayer b_layer, CollisionMask b_mask)
	{
		return (a_layer & b_mask) != 0u && (b_layer & a_mask) != 0u;
	}
}

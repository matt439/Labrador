#pragma once

#include <cstdint>

namespace labrador
{
	// Which group an object belongs to: exactly one bit, set by the game.
	//
	// The engine assigns no meaning to any bit and never compares one to a
	// named constant - there are no named constants here, and that absence is
	// the point. An enum of one game's nouns reachable from the collision
	// interface would mean the engine could not be handed a second game
	// without either editing the enum or lying to it (T1).
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
	// what makes the answer a property of the pair: a one-sided filter can
	// pass (a, b) and fail (b, a), and which response runs then comes down to
	// which object a loop happens to be iterating.
	constexpr bool layers_collide(CollisionLayer a_layer, CollisionMask a_mask,
		CollisionLayer b_layer, CollisionMask b_mask)
	{
		return (a_layer & b_mask) != 0u && (b_layer & a_mask) != 0u;
	}
}

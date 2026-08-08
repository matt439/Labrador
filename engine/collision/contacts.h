#pragma once

#include "engine/math/matt_math.h"

#include <span>
#include <vector>

namespace artattack
{
	class CollisionObject;

	// One overlapping pair, and the smallest translation that separates it.
	//
	// The frame's contacts are a list the caller can look at before anything
	// responds to them - which is the whole reason this is a value and not a
	// callback fired from inside the sweep. A test can assert on the list, and
	// a scene can hand it to the game after resolution instead of during it.
	struct Contact
	{
		CollisionObject* a = nullptr;
		CollisionObject* b = nullptr;

		// Unit, pointing from `a` towards `b`.
		mattmath::Vector2F normal = mattmath::Vector2F::ZERO;

		// Overlap along `normal`, always greater than zero.
		float penetration = 0.0f;
	};

	// Fills `contacts` with every overlapping pair among `objects`, each pair
	// once.
	//
	// `contacts` is cleared first and reused, so a caller that keeps one
	// vector across frames allocates nothing after the first busy frame -
	// which is the point of taking it by reference rather than returning it.
	//
	// Three filters, cheapest first: layer/mask, then the shapes' bounding
	// boxes, then narrow_phase. Objects already flagged for deletion take part
	// in nothing.
	//
	// The pair enumeration is still all-pairs. That is the honest state of it:
	// PHILOSOPHY promises a broad phase that prunes pairs, and this function
	// is where one goes - the signature does not change when it arrives,
	// because "which pairs are worth measuring" is exactly the question the
	// two cheap filters above already answer badly.
	//
	// What it replaces: two hand-written nested loops that tested (player,
	// object) and then (object, player) and then (object, object) with both
	// orderings, so most pairs were measured twice and every pair's response
	// depended on which loop reached it first.
	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts);

	// Tells both participants of every contact, once each, with the normal
	// oriented for the object receiving it.
	//
	// A contact whose participant was retired by an earlier response in the
	// same frame is dropped - a projectile that hits a wall does not go on to
	// hit the player behind it. That rule used to be a `continue` inside the
	// sweep, so whether it applied depended on iteration order; here it
	// applies to both sides of every pair.
	void dispatch_contacts(std::span<const Contact> contacts);
}

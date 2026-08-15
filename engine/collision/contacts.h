#pragma once

#include "engine/math/vector2f.h"

#include <span>
#include <vector>

namespace labrador
{
	class BroadPhase;
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
	// With no broad phase, the pair enumeration is all-pairs - O(n^2), which
	// bench/ measures at 106 ms for four thousand objects, six times a 60 Hz
	// frame from one call. Pass one and it enumerates only the pairs whose
	// bounding boxes overlap, in the same order.
	//
	// The parameter is a pointer with a default rather than a required
	// argument because the exhaustive sweep is the specification: it is what
	// the grid is tested against (tests/collision/broad_phase_tests.cpp), and
	// a caller with a handful of objects has no reason to build a grid.
	//
	// `broad_phase` is borrowed, and it holds the buffers that make the steady
	// state allocation-free, so a caller that wants that keeps one across
	// frames - exactly as it does with `contacts`.
	//
	// What it replaces: two hand-written nested loops that tested (player,
	// object) and then (object, player) and then (object, object) with both
	// orderings, so most pairs were measured twice and every pair's response
	// depended on which loop reached it first.
	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts,
		BroadPhase* broad_phase = nullptr);

	// Tells both participants of every contact, once each, with the normal
	// oriented for the object receiving it.
	//
	// Two things a response does are accounted for here, because they are
	// properties of dispatching a list rather than of any one response:
	//
	//   - A response can retire an object, and a retired object's remaining
	//     contacts are dropped. A projectile that hits a wall does not go on
	//     to hit the player behind it. That rule used to be a `continue`
	//     inside the sweep, so whether it applied depended on which of two
	//     nested loops reached the pair; here it applies to both sides of
	//     every pair.
	//   - A response can move an object, so every pair is measured again
	//     immediately before it is dispatched, and a pair an earlier response
	//     has already separated is dropped. The depths in the list describe
	//     the frame before anything responded to it, which is what a caller
	//     inspecting the list wants and not what a caller acting on it does.
	void dispatch_contacts(std::span<const Contact> contacts);
}

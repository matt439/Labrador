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

	// What one sweep did, in work units rather than in time.
	//
	// These are the terms of Ericson's hierarchy cost model (Real-Time
	// Collision Detection, 6.1.2): the total cost of a query is the number of
	// bounding-volume tests times what one costs, plus the number of primitive
	// tests times what one of those costs. The counts are the half of that
	// product an algorithm controls; the per-test costs are the half a
	// profiler measures.
	//
	// They are reported because they are *pinnable*. A wall-clock figure
	// cannot be asserted on in a test - it moves with the machine, the build
	// type and the weather - but "this scene enumerates 499,500 pairs" is an
	// integer, and a broad phase that fails to reduce it has failed
	// observably. PHILOSOPHY (Performance) asks that a throughput regression
	// be a defect rather than a curiosity; this is what a test can hold.
	struct SweepCounts
	{
		// Pairs the enumeration offered to the filters, counting only pairs
		// where both objects were live. This is the number a broad phase
		// exists to reduce, and today it is n * (n - 1) / 2 - every pair of
		// every object, however far apart.
		long long pairs_enumerated = 0;

		// Bounding-volume tests actually run: pairs that got past layer and
		// mask filtering and reached Shape::AABB_intersects. Ericson's Nv.
		long long bound_tests = 0;

		// Primitive tests actually run: pairs whose bounds overlapped and so
		// reached narrow_phase. Ericson's Np, and much the more expensive of
		// the two.
		long long narrow_tests = 0;
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

	// The same sweep, reporting what it cost. `counts` is overwritten, not
	// accumulated, so a caller measuring several frames adds them up itself.
	//
	// There is one implementation and the two-argument form calls this one
	// with a discarded total. That costs three integer increments per pair on
	// a path that already makes a virtual call and tests two boxes, which is
	// worth paying: two implementations of a sweep would drift, and the
	// version under test would stop being the version that ships (T4).
	void find_contacts(std::span<CollisionObject* const> objects,
		std::vector<Contact>& contacts, SweepCounts& counts);

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

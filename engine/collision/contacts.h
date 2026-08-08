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
		// Pairs the broad phase offered to the filters, counting only pairs
		// where both objects were live. Before the sweep existed this was
		// n * (n - 1) / 2 - every pair of every object, however far apart -
		// and the gap between that figure and this one is what the broad
		// phase is worth on a given scene.
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
	// Four filters, cheapest first: a broad-phase sweep, then layer/mask, then
	// the shapes' bounding boxes, then narrow_phase. Objects already flagged
	// for deletion take part in nothing.
	//
	// The broad phase is a one-axis sort and sweep (Ericson, Real-Time
	// Collision Detection, 7.5). Every live object's bounding box is reduced
	// to a flat POD interval once per frame, the array is sorted on the
	// interval minimum, and each object is then compared only with those that
	// begin before it ends. A grid was the alternative and was rejected on
	// this content: level geometry runs from a 20-unit paint tile to a
	// level-spanning platform, and no single cell size serves both.
	//
	// The axis is chosen each frame as the one whose object centres are more
	// spread out, so a tall level sweeps vertically without being told to.
	//
	// What this is NOT, recorded so the alternatives are not re-derived from
	// the same chapter. Not a uniform grid (7.1): level geometry runs from a
	// 20-unit paint tile to a level-spanning platform, and no cell size serves
	// both - a grid sized for the tile makes the platform occupy hundreds of
	// cells, and one sized for the platform puts every tile in one bucket.
	// Not a hierarchical grid, quadtree, k-d tree or BVH (7.2, 7.3, ch.6):
	// every one of those is a pointer-linked structure to be rebuilt or
	// refitted each frame, and this content has no depth for them to exploit.
	// Not a BSP tree (ch.8): it answers "what is along this ray, in order",
	// which is a question this engine does not ask. Objects are partitioned;
	// shapes are never split, and what is stored is an index, never a
	// fragment.
	//
	// The one improvement this shape does admit is an index per layer group.
	// The counts say why: in a level of paint tiles, the overwhelming majority
	// of pairs the sweep offers are tile-against-tile and die on the layer
	// filter one line later, so the sweep is re-deriving what the layer bits
	// already knew. That needs an index per group rather than one array, which
	// is a design decision and not a loop change.
	//
	// The filters run in one direction and it matters which. Every cheap test
	// here is CLOSED - boxes that share only an edge count as intersecting -
	// and narrow_phase, which owns the answer, is OPEN, because a zero-depth
	// contact has no meaningful normal. That pairing is the correct one: a
	// filter may pass something the decider will reject, but it must never
	// reject something the decider would have accepted. Inverting either
	// boundary turns a cheap optimisation into missed collisions along every
	// surface a player walks on, and it would do so only for pairs that are
	// exactly touching - which is to say, intermittently, and mostly when
	// something has come to rest.
	//
	// **The order of `contacts` is unspecified**, and depends on the sweep.
	// Every overlapping pair appears exactly once and each Contact names its
	// participants in the order the caller listed them, so `a` and `normal`
	// mean what they always meant - but do not index this vector expecting a
	// particular pair, and do not let a test do it either.
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

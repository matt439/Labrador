#pragma once

#include "engine/math/rectanglef.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace artattack
{
	class CollisionObject;

	// Which pairs are worth measuring.
	//
	// PHILOSOPHY's Rendering and Collision sections both promise this, and
	// find_contacts' own comment named its absence: the pair enumeration was
	// all-pairs, so the sweep was O(n^2) in the object count. bench/ says what
	// that costs. On a release build, Scene::resolve is 25 us at 64 objects,
	// 380 us at 256, 6.1 ms at 1,024 and 106 ms at 4,096 - which is six times
	// the whole 60 Hz frame budget, from one call, at an object count a 2D
	// engine has no business finding difficult.
	//
	// The same benchmark is why there is no spatial index in front of the
	// *render* cull, which is where the plan expected one to go. That loop is
	// 6.2 ns per object and flat from 64 to 4,096 - 102 us for four views over
	// four thousand objects, well under one percent of a frame. An index there
	// would be ceremony. The measurement moved the work; that is what it was
	// for.
	//
	// WHY A UNIFORM GRID. A paint arena is a plane of structures of broadly
	// similar size, mostly not touching, with players and projectiles moving
	// through them. That is the case a uniform grid is best at and a tree is
	// worst at: no rebalancing, no per-frame allocation once the buffers are
	// warm, and cell membership is arithmetic rather than a descent. Where it
	// degrades - one object far larger than the rest - it degrades towards the
	// all-pairs sweep it replaces rather than towards anything worse, and the
	// large-object list below bounds even that.
	//
	// WHAT IT GUARANTEES. Exactly the pairs the all-pairs sweep would have
	// considered, minus ones whose bounding boxes cannot overlap, in the same
	// ascending (a, b) index order. Same order matters: a contact list is a
	// value the game reads, and dispatch re-measures each pair in list order,
	// so a different order is a different resolution. tests/collision asserts
	// the two enumerations agree object-for-object on randomised scenes.
	class BroadPhase
	{
	public:
		BroadPhase();

		// Fills `pairs` with the index pairs worth measuring, ascending by
		// first index then second, with no duplicates.
		//
		// `pairs` is cleared and reused, and so is every buffer inside this
		// object - which is what makes the steady state allocation-free
		// (PHILOSOPHY, Performance: per-frame code performs no heap
		// allocation). Objects flagged for deletion are skipped here rather
		// than by the caller, because a pair that cannot produce a contact
		// should not reach the grid at all.
		void find_pairs(std::span<CollisionObject* const> objects,
			std::vector<std::pair<int, int>>& pairs);

		// The cell edge the last call chose, in world units. For tests and for
		// anyone wondering why a scene got slow.
		float cell_size() const;

		// How many objects the last call put in the large-object list - the
		// ones whose bounding box covers so much of the grid that indexing
		// them costs more than testing them against everything.
		int large_object_count() const;

		// The pair list this object keeps across frames, so find_contacts has
		// somewhere allocation-free to receive one. A caller driving
		// find_pairs itself may pass its own vector instead.
		std::vector<std::pair<int, int>>& pairs_buffer();

	private:
		// Cells are addressed by a packed pair of 32-bit signed coordinates,
		// so the grid is sparse and the world needs no origin or extent.
		using CellKey = uint64_t;

		struct Entry
		{
			CellKey key = 0;
			int index = 0;
		};

		void clear_buffers();

		// Sorted by key, so every cell's members are a contiguous run.
		std::vector<Entry> entries_;

		// Objects the grid refused, tested against every other object.
		std::vector<int> large_;

		// Live objects, in the caller's indexing.
		std::vector<int> candidates_;
		std::vector<mattmath::RectangleF> boxes_;

		// Kept here so find_contacts allocates nothing per frame.
		std::vector<std::pair<int, int>> pairs_;

		float cell_size_ = 0.0f;
	};
}

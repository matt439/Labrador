#include "engine/collision/broad_phase.h"

#include "engine/collision/collision_object.h"

#include <algorithm>
#include <cmath>

using mattmath::RectangleF;

namespace artattack
{
	namespace
	{
		// How many cells one object may be indexed into before it is cheaper
		// to test it against everything.
		//
		// A structure the size of the arena would otherwise be inserted into
		// every cell of the grid, which is the all-pairs sweep plus the cost
		// of building a grid first. Twenty-five is generous for the case this
		// is for - an object a few times the size of its neighbours - and
		// small enough that the pathological one never gets there.
		constexpr int MAX_CELLS_PER_OBJECT = 25;

		// Cell edge as a multiple of the mean object extent.
		//
		// Two is the standard choice and the benchmark agrees with it: at one,
		// objects straddle cells constantly and the duplicate-pair count
		// climbs; at four, cells hold enough members that the within-cell
		// comparison starts to look quadratic again.
		constexpr float CELL_SIZE_FACTOR = 2.0f;

		// A scene where every object sits at the same point has a mean extent
		// of zero, and a cell size of zero is a division by it.
		constexpr float MIN_CELL_SIZE = 1.0f;

		uint64_t pack(int32_t x, int32_t y)
		{
			return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
				static_cast<uint64_t>(static_cast<uint32_t>(y));
		}

		int32_t cell_of(float value, float cell_size)
		{
			return static_cast<int32_t>(std::floor(value / cell_size));
		}
	}

	BroadPhase::BroadPhase() = default;

	float BroadPhase::cell_size() const
	{
		return this->cell_size_;
	}

	std::vector<std::pair<int, int>>& BroadPhase::pairs_buffer()
	{
		return this->pairs_;
	}

	int BroadPhase::large_object_count() const
	{
		return static_cast<int>(this->large_.size());
	}

	void BroadPhase::clear_buffers()
	{
		this->entries_.clear();
		this->large_.clear();
		this->candidates_.clear();
		this->boxes_.clear();
	}

	void BroadPhase::find_pairs(std::span<CollisionObject* const> objects,
		std::vector<std::pair<int, int>>& pairs)
	{
		pairs.clear();
		this->clear_buffers();

		// Every object that could take part, and its bounding box read once.
		//
		// Once, and not once per pair: shape()->bounding_box() is a virtual
		// call that computes, and the all-pairs sweep called it O(n) times per
		// object. Reading them into a flat vector here is also what makes the
		// grid build a walk over contiguous memory.
		this->candidates_.reserve(objects.size());
		this->boxes_.reserve(objects.size());

		for (size_t i = 0; i < objects.size(); i++)
		{
			CollisionObject* const object = objects[i];
			if (object == nullptr || object->for_deletion())
			{
				continue;
			}
			this->candidates_.push_back(static_cast<int>(i));
			this->boxes_.push_back(object->shape()->bounding_box());
		}

		const size_t count = this->candidates_.size();
		if (count < 2)
		{
			return;
		}

		// The cell edge, from the mean extent of what is actually in the
		// scene. A grid sized against a constant would be wrong for every
		// scene except the one it was tuned on.
		double extent_total = 0.0;
		for (const RectangleF& box : this->boxes_)
		{
			extent_total += static_cast<double>(box.width) +
				static_cast<double>(box.height);
		}
		const float mean_extent =
			static_cast<float>(extent_total / (2.0 * static_cast<double>(count)));
		this->cell_size_ =
			std::max(MIN_CELL_SIZE, mean_extent * CELL_SIZE_FACTOR);

		// Index each object into the cells its box touches, or set it aside.
		this->entries_.reserve(count * 4);
		for (size_t c = 0; c < count; c++)
		{
			const RectangleF& box = this->boxes_[c];

			const int32_t min_x = cell_of(box.left(), this->cell_size_);
			const int32_t max_x = cell_of(box.right(), this->cell_size_);
			const int32_t min_y = cell_of(box.top(), this->cell_size_);
			const int32_t max_y = cell_of(box.bottom(), this->cell_size_);

			const int64_t spanned =
				static_cast<int64_t>(max_x - min_x + 1) *
				static_cast<int64_t>(max_y - min_y + 1);

			if (spanned > MAX_CELLS_PER_OBJECT)
			{
				this->large_.push_back(static_cast<int>(c));
				continue;
			}

			for (int32_t y = min_y; y <= max_y; y++)
			{
				for (int32_t x = min_x; x <= max_x; x++)
				{
					this->entries_.push_back(
						Entry{ pack(x, y), static_cast<int>(c) });
				}
			}
		}

		// Sorted by cell, so each cell is a contiguous run and there is no map
		// and no per-cell allocation.
		std::sort(this->entries_.begin(), this->entries_.end(),
			[](const Entry& a, const Entry& b)
			{
				if (a.key != b.key) { return a.key < b.key; }
				return a.index < b.index;
			});

		// Within-cell pairs. An object in two cells with another object in
		// both produces the pair twice; the sort and unique below is what
		// removes that, rather than a per-pair set lookup.
		for (size_t start = 0; start < this->entries_.size();)
		{
			size_t end = start + 1;
			while (end < this->entries_.size() &&
				this->entries_[end].key == this->entries_[start].key)
			{
				end++;
			}

			for (size_t i = start; i < end; i++)
			{
				for (size_t j = i + 1; j < end; j++)
				{
					const int a = this->entries_[i].index;
					const int b = this->entries_[j].index;

					// The boxes have to actually overlap. Sharing a cell only
					// says they are near each other, and emitting the pair
					// anyway would hand find_contacts work the all-pairs sweep
					// would have rejected on its own second filter.
					if (!this->boxes_[static_cast<size_t>(a)].intersects(
						this->boxes_[static_cast<size_t>(b)]))
					{
						continue;
					}

					pairs.emplace_back(
						this->candidates_[static_cast<size_t>(a)],
						this->candidates_[static_cast<size_t>(b)]);
				}
			}

			start = end;
		}

		// The objects the grid refused, against everything.
		for (int large : this->large_)
		{
			for (size_t other = 0; other < count; other++)
			{
				if (static_cast<int>(other) == large)
				{
					continue;
				}
				if (!this->boxes_[static_cast<size_t>(large)].intersects(
					this->boxes_[other]))
				{
					continue;
				}

				const int a = this->candidates_[static_cast<size_t>(large)];
				const int b = this->candidates_[other];
				pairs.emplace_back(std::min(a, b), std::max(a, b));
			}
		}

		// Ascending, and each pair once.
		//
		// The order is the contract, not a convenience: dispatch re-measures
		// each pair in list order and a response moves things, so enumerating
		// the same pairs in a different order is a different resolution. This
		// is the line that makes the grid's output identical to the sweep's.
		std::sort(pairs.begin(), pairs.end());
		pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
	}
}

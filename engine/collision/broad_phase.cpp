#include "engine/collision/broad_phase.h"

#include "engine/collision/collision_object.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace labrador
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
		constexpr int max_cells_per_object = 25;

		// Cell edge as a multiple of the mean object extent.
		//
		// Two is the standard choice and the benchmark agrees with it: at one,
		// objects straddle cells constantly and the duplicate-pair count
		// climbs; at four, cells hold enough members that the within-cell
		// comparison starts to look quadratic again.
		constexpr float cell_size_factor = 2.0f;

		// A scene where every object sits at the same point has a mean extent
		// of zero, and a cell size of zero is a division by it.
		constexpr float min_cell_size = 1.0f;

		uint64_t pack(int32_t x, int32_t y)
		{
			return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
				static_cast<uint64_t>(static_cast<uint32_t>(y));
		}

		// The cell a coordinate falls in, clamped to the range a cell key can
		// address.
		//
		// Divided in double and clamped before the conversion, because a
		// float-to-int32 conversion of a value outside that range is undefined
		// behaviour, and a finite box can still have an infinite edge where
		// x + width overflows. Clamping is monotonic, so two boxes that
		// overlap still share a cell; boxes far apart that clamp to the same
		// edge cell are separated by the box test every pair passes before it
		// is emitted. A NaN cannot reach here (collect_candidates sets those
		// boxes aside), but it maps to the lowest cell rather than to
		// undefined behaviour.
		int32_t cell_of(float value, float cell_size)
		{
			const double cell = std::floor(
				static_cast<double>(value) / static_cast<double>(cell_size));
			constexpr double lowest =
				static_cast<double>(std::numeric_limits<int32_t>::min());
			constexpr double highest =
				static_cast<double>(std::numeric_limits<int32_t>::max());
			if (!(cell > lowest))
			{
				return std::numeric_limits<int32_t>::min();
			}
			if (cell >= highest)
			{
				return std::numeric_limits<int32_t>::max();
			}
			return static_cast<int32_t>(cell);
		}

		bool is_finite(const RectangleF& box)
		{
			return std::isfinite(box.x) && std::isfinite(box.y)
				&& std::isfinite(box.width) && std::isfinite(box.height);
		}

		bool has_nan(const RectangleF& box)
		{
			return std::isnan(box.x) || std::isnan(box.y)
				|| std::isnan(box.width) || std::isnan(box.height);
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

		this->collect_candidates(objects);
		if (this->candidates_.size() < 2)
		{
			return;
		}

		this->choose_cell_size();
		this->index_cells();
		this->append_cell_pairs(pairs);
		this->append_large_pairs(pairs);

		// Ascending, and each pair once.
		//
		// The order is the contract, not a convenience: dispatch re-measures
		// each pair in list order and a response moves things, so enumerating
		// the same pairs in a different order is a different resolution. This
		// is the line that makes the grid's output identical to the sweep's.
		std::sort(pairs.begin(), pairs.end());
		pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
	}

	// Every object that could take part, and its bounding box read once.
	//
	// Once, and not once per pair: shape()->bounding_box() is a virtual call
	// that computes, and the all-pairs sweep called it O(n) times per object.
	// Reading them into a flat vector here is also what makes the grid build a
	// walk over contiguous memory.
	void BroadPhase::collect_candidates(
		std::span<CollisionObject* const> objects)
	{
		this->candidates_.reserve(objects.size());
		this->boxes_.reserve(objects.size());

		for (size_t i = 0; i < objects.size(); i++)
		{
			CollisionObject* const object = objects[i];
			if (object == nullptr || object->for_deletion())
			{
				continue;
			}

			// A box with a NaN field intersects nothing (test_AABB_AABB), so
			// the sweep can never measure it and neither does the grid.
			const RectangleF box = object->shape()->bounding_box();
			if (has_nan(box))
			{
				continue;
			}
			this->candidates_.push_back(static_cast<int>(i));
			this->boxes_.push_back(box);
		}
	}

	// The cell edge, from the mean extent of what is actually in the scene. A
	// grid sized against a constant would be wrong for every scene except the
	// one it was tuned on.
	//
	// Only finite boxes count towards the mean: one infinite extent would
	// make every cell infinite. The size is computed in double and clamped to
	// the largest float, which finite extents near that limit would
	// otherwise overflow.
	void BroadPhase::choose_cell_size()
	{
		double extent_total = 0.0;
		size_t finite_count = 0;
		for (const RectangleF& box : this->boxes_)
		{
			if (!is_finite(box))
			{
				continue;
			}
			extent_total += static_cast<double>(box.width) +
				static_cast<double>(box.height);
			finite_count++;
		}
		if (finite_count == 0)
		{
			this->cell_size_ = min_cell_size;
			return;
		}
		const double mean_extent =
			extent_total / (2.0 * static_cast<double>(finite_count));
		const double cell_size = std::min(
			mean_extent * static_cast<double>(cell_size_factor),
			static_cast<double>(std::numeric_limits<float>::max()));
		this->cell_size_ =
			std::max(min_cell_size, static_cast<float>(cell_size));
	}

	// Each object into the cells its box touches, or set aside as large, and
	// then sorted by cell, so each cell is a contiguous run and there is no
	// map and no per-cell allocation.
	void BroadPhase::index_cells()
	{
		const size_t count = this->candidates_.size();
		this->entries_.reserve(count * 4);
		for (size_t c = 0; c < count; c++)
		{
			const RectangleF& box = this->boxes_[c];

			// An infinite field has no cell range, but its box can still
			// overlap others - a circle whose doubled radius overflows has a
			// finite centre and radius and real contacts - so it is tested
			// against everything rather than dropped.
			if (!is_finite(box))
			{
				this->large_.push_back(static_cast<int>(c));
				continue;
			}

			const int32_t min_x = cell_of(box.left(), this->cell_size_);
			const int32_t max_x = cell_of(box.right(), this->cell_size_);
			const int32_t min_y = cell_of(box.top(), this->cell_size_);
			const int32_t max_y = cell_of(box.bottom(), this->cell_size_);

			// In 64 bits, and each axis bounded before the product: a span
			// across the whole clamped range is 2^32 cells, and two of those
			// multiplied overflow even 64 bits. A span below one is a box
			// built inside out, with a negative extent; it covers no cell
			// range but can still intersect a box, so it goes to the list
			// that tests everything too.
			const int64_t span_x =
				static_cast<int64_t>(max_x) - static_cast<int64_t>(min_x) + 1;
			const int64_t span_y =
				static_cast<int64_t>(max_y) - static_cast<int64_t>(min_y) + 1;

			if (span_x < 1 || span_y < 1 ||
				span_x > max_cells_per_object ||
				span_y > max_cells_per_object ||
				span_x * span_y > max_cells_per_object)
			{
				this->large_.push_back(static_cast<int>(c));
				continue;
			}

			// The counters are 64-bit so that a box in the last cell, at the
			// top of the int32 range, ends the loop instead of overflowing it.
			for (int64_t y = min_y; y <= max_y; y++)
			{
				for (int64_t x = min_x; x <= max_x; x++)
				{
					this->entries_.push_back(Entry{
						pack(static_cast<int32_t>(x), static_cast<int32_t>(y)),
						static_cast<int>(c) });
				}
			}
		}

		std::sort(this->entries_.begin(), this->entries_.end(),
			[](const Entry& a, const Entry& b)
			{
				if (a.key != b.key) { return a.key < b.key; }
				return a.index < b.index;
			});
	}

	// Within-cell pairs. An object in two cells with another object in both
	// produces the pair twice; find_pairs' sort and unique is what removes
	// that, rather than a per-pair set lookup.
	void BroadPhase::append_cell_pairs(
		std::vector<std::pair<int, int>>& pairs) const
	{
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
	}

	// The objects the grid refused, against everything.
	void BroadPhase::append_large_pairs(
		std::vector<std::pair<int, int>>& pairs) const
	{
		const size_t count = this->candidates_.size();
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
	}
}

#pragma once

#include <utility>
#include <vector>

namespace labrador
{
	// Splits a count of elements into contiguous index ranges, one per
	// worker, so that a ThreadPool's tasks each take a slice of one list.
	// Scene::draw slices its views with one; it holds no state, so one can
	// serve any number of callers at once.
	class Partitioner
	{
	public:
		Partitioner() = default;

		// Splits [0, element_count) into at most partition_count contiguous
		// half-open ranges, [first, second), covering every element exactly
		// once. The remainder is spread one element at a time over the
		// leading ranges rather than piled onto the last, so no range is more
		// than one element longer than any other.
		//
		// Fewer ranges than asked for when there are fewer elements than
		// partitions: an empty range is work for a worker that has none, so
		// element_count == 0 returns no ranges at all.
		//
		// The ranges replace the contents of `ranges`, whose capacity is
		// reused: a caller that keeps the vector across frames allocates
		// nothing once it has held as many ranges as it is asked for
		// (PHILOSOPHY, Performance: per-frame code performs no heap
		// allocation).
		//
		// Preconditions, checked: partition_count >= 1 and element_count >= 0.
		// partition_count is a divisor on the first line, so this function
		// checks it itself rather than trusting a caller's earlier check. A
		// failed check throws std::invalid_argument and leaves `ranges`
		// unchanged.
		void partition(int element_count, int partition_count,
			std::vector<std::pair<int, int>>& ranges) const;

		void partition(size_t element_count, int partition_count,
			std::vector<std::pair<int, int>>& ranges) const;
	};
}

#pragma once

#include <utility>
#include <vector>

namespace labrador
{
	class Partitioner
	{
	public:
		Partitioner() = default;

		// Splits [0, num_elements) into at most num_partitions contiguous
		// half-open ranges, [first, second), covering every element exactly
		// once. The remainder is spread one element at a time over the
		// leading ranges rather than piled onto the last, so no range is more
		// than one element longer than any other.
		//
		// Fewer ranges than asked for when there are fewer elements than
		// partitions: an empty range is work for a worker that has none, so
		// num_elements == 0 returns no ranges at all.
		//
		// The ranges replace the contents of `ranges`, whose capacity is
		// reused: a caller that keeps the vector across frames allocates
		// nothing once it has held as many ranges as it is asked for
		// (PHILOSOPHY, Performance: per-frame code performs no heap
		// allocation).
		//
		// Preconditions, checked: num_partitions >= 1 and num_elements >= 0.
		// num_partitions is a divisor on the first line, and it arrives from
		// ApplicationOptions::max_threads by way of ThreadPool, and unchecked
		// anywhere on that route a zero in a config file is an integer
		// divide-by-zero on the first frame of the first match. A failed
		// check throws std::invalid_argument and leaves `ranges` unchanged.
		void partition(int num_elements, int num_partitions,
			std::vector<std::pair<int, int>>& ranges) const;

		void partition(size_t num_elements, int num_partitions,
			std::vector<std::pair<int, int>>& ranges) const;
	};
}

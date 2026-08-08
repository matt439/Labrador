#pragma once

#include <utility>
#include <vector>

namespace artattack
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
		// Preconditions, checked: num_partitions >= 1 and num_elements >= 0.
		// num_partitions is a divisor on the first line, and it arrives from
		// ApplicationOptions::max_threads by way of ThreadPool, neither of
		// which used to validate it - so a zero in a config file was an
		// integer divide-by-zero on the first frame of the first match.
		std::vector<std::pair<int, int>>
			partition(int num_elements, int num_partitions) const;

		std::vector<std::pair<int, int>>
			partition(size_t num_elements, int num_partitions) const;
	};
}

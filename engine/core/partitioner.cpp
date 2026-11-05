#include "engine/core/partitioner.h"

#include <limits>
#include <stdexcept>
#include <vector>

namespace labrador
{
	void Partitioner::partition(int element_count, int partition_count,
		std::vector<std::pair<int, int>>& ranges) const
	{
		if (partition_count < 1)
		{
			throw std::invalid_argument(
				"Partitioner::partition: partition_count must be at least 1.");
		}
		if (element_count < 0)
		{
			throw std::invalid_argument(
				"Partitioner::partition: element_count must not be negative.");
		}

		ranges.clear();
		int elements_per_partition = element_count / partition_count;
		int remainder = element_count % partition_count;
		int start = 0;
		for (int i = 0; i < partition_count && i < element_count; i++)
		{
			int end = start + elements_per_partition;
			if (remainder > 0)
			{
				end++;
				remainder--;
			}
			ranges.push_back(std::make_pair(start, end));
			start = end;
		}
	}

	void Partitioner::partition(size_t element_count, int partition_count,
		std::vector<std::pair<int, int>>& ranges) const
	{
		// The int overload's ranges are ints, so a count that does not fit in
		// one cannot be answered - and narrowing it silently would hand back
		// ranges that do not cover the collection.
		if (element_count >
			static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			throw std::invalid_argument(
				"Partitioner::partition: element_count exceeds the range this "
				"partitioner can address.");
		}
		Partitioner::partition(static_cast<int>(element_count), partition_count,
			ranges);
	}
}

#include "engine/collision/partitioner.h"

#include <limits>
#include <stdexcept>
#include <vector>

namespace artattack
{
	std::vector<std::pair<int, int>> Partitioner::partition(int num_elements, int num_partitions) const
	{
		if (num_partitions < 1)
		{
			throw std::invalid_argument(
				"Partitioner::partition: num_partitions must be at least 1.");
		}
		if (num_elements < 0)
		{
			throw std::invalid_argument(
				"Partitioner::partition: num_elements must not be negative.");
		}

		std::vector<std::pair<int, int>> result;
		int elements_per_partition = num_elements / num_partitions;
		int remainder = num_elements % num_partitions;
		int start = 0;
		for (int i = 0; i < num_partitions && i < num_elements; i++)
		{
			int end = start + elements_per_partition;
			if (remainder > 0)
			{
				end++;
				remainder--;
			}
			result.push_back(std::make_pair(start, end));
			start = end;
		}
		return result;
	}

	std::vector<std::pair<int, int>> Partitioner::partition(size_t num_elements, int num_partitions) const
	{
		// The int overload's ranges are ints, so a count that does not fit in
		// one cannot be answered - and narrowing it silently would hand back
		// ranges that do not cover the collection.
		if (num_elements >
			static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			throw std::invalid_argument(
				"Partitioner::partition: num_elements exceeds the range this "
				"partitioner can address.");
		}
		return Partitioner::partition(static_cast<int>(num_elements), num_partitions);
	}
}

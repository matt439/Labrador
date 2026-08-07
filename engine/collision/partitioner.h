#pragma once

#include <vector>

namespace artattack
{
	class Partitioner
	{
	public:
		Partitioner() = default;

		std::vector<std::pair<int, int>>
			partition(int num_elements, int num_partitions) const;

		std::vector<std::pair<int, int>>
			partition(size_t num_elements, int num_partitions) const;
	};
}

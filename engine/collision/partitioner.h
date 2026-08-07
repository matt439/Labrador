#ifndef PARTITIONER_H
#define PARTITIONER_H

#include <vector>

class Partitioner
{
public:
	Partitioner() = default;

	std::vector<std::pair<int, int>>
		partition(int num_elements, int num_partitions) const;

	std::vector<std::pair<int, int>>
		partition(size_t num_elements, int num_partitions) const;
};

#endif // !PARTITIONER_H

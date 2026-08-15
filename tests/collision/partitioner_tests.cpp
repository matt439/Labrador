#include <doctest/doctest.h>

#include "engine/collision/partitioner.h"

#include <stdexcept>
#include <vector>

using labrador::Partitioner;

namespace
{
	using Ranges = std::vector<std::pair<int, int>>;

	// The property that matters to every caller: the ranges are contiguous,
	// they start at zero, they end at num_elements, and together they name
	// every element exactly once.
	bool covers_exactly(const Ranges& ranges, int num_elements)
	{
		int next = 0;
		for (const auto& range : ranges)
		{
			if (range.first != next || range.second < range.first)
			{
				return false;
			}
			next = range.second;
		}
		return next == num_elements;
	}
}

TEST_CASE("an exact division gives equal ranges")
{
	const Partitioner partitioner;
	const Ranges ranges = partitioner.partition(12, 4);

	CHECK(ranges == Ranges{ {0, 3}, {3, 6}, {6, 9}, {9, 12} });
	CHECK(covers_exactly(ranges, 12));
}

TEST_CASE("the remainder is spread over the leading ranges, not piled on the last")
{
	const Partitioner partitioner;
	const Ranges ranges = partitioner.partition(10, 4);

	CHECK(ranges == Ranges{ {0, 3}, {3, 6}, {6, 8}, {8, 10} });
	CHECK(covers_exactly(ranges, 10));
}

TEST_CASE("fewer elements than partitions gives one range each and no empties")
{
	const Partitioner partitioner;
	const Ranges ranges = partitioner.partition(2, 8);

	CHECK(ranges == Ranges{ {0, 1}, {1, 2} });
	CHECK(covers_exactly(ranges, 2));
}

TEST_CASE("no elements gives no ranges")
{
	const Partitioner partitioner;
	CHECK(partitioner.partition(0, 4).empty());
	CHECK(partitioner.partition(size_t{ 0 }, 4).empty());
}

TEST_CASE("one partition gives the whole range")
{
	const Partitioner partitioner;
	CHECK(partitioner.partition(7, 1) == Ranges{ {0, 7} });
}

TEST_CASE("coverage holds across every element count and partition count")
{
	const Partitioner partitioner;
	for (int elements = 0; elements <= 40; elements++)
	{
		for (int partitions = 1; partitions <= 17; partitions++)
		{
			const Ranges ranges = partitioner.partition(elements, partitions);

			CAPTURE(elements);
			CAPTURE(partitions);
			REQUIRE(covers_exactly(ranges, elements));
			REQUIRE(static_cast<int>(ranges.size()) <= partitions);

			// No range more than one longer than any other.
			int shortest = elements + 1;
			int longest = 0;
			for (const auto& range : ranges)
			{
				const int length = range.second - range.first;
				shortest = std::min(shortest, length);
				longest = std::max(longest, length);
			}
			if (!ranges.empty())
			{
				REQUIRE(longest - shortest <= 1);
			}
		}
	}
}

TEST_CASE("a partition count of zero is rejected rather than divided by")
{
	const Partitioner partitioner;

	// This is the live path: ApplicationOptions::max_threads reaches here
	// through ThreadPool, on every frame of every view.
	CHECK_THROWS_AS(partitioner.partition(10, 0), std::invalid_argument);
	CHECK_THROWS_AS(partitioner.partition(10, -1), std::invalid_argument);
	CHECK_THROWS_AS(partitioner.partition(size_t{ 10 }, 0), std::invalid_argument);
}

TEST_CASE("a negative element count is rejected")
{
	const Partitioner partitioner;
	CHECK_THROWS_AS(partitioner.partition(-1, 4), std::invalid_argument);
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bench
{
	// A distribution reported without interpolation. Frame tails are observed
	// samples, not values invented between two of them, so every percentile is
	// the nearest-rank order statistic.
	struct FrameSummary
	{
		std::size_t count = 0;
		std::int64_t minimum = 0;
		std::int64_t p50 = 0;
		std::int64_t p95 = 0;
		std::int64_t p99 = 0;
		std::int64_t maximum = 0;
	};

	// Percent is in [1, 100]. Throws std::invalid_argument for an empty sample
	// or a percentile outside that range.
	std::int64_t nearest_rank(const std::vector<std::int64_t>& samples,
		unsigned int percent);

	// Sorts one copy and returns the five fixed order statistics written to the
	// benchmark artifact.
	FrameSummary summarize_frames(const std::vector<std::int64_t>& samples);
}

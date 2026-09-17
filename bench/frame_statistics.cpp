#include "bench/frame_statistics.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace bench
{
	namespace
	{
		std::size_t rank_index(std::size_t count, unsigned int percent)
		{
			if (count == 0)
			{
				throw std::invalid_argument(
					"nearest_rank requires at least one sample.");
			}
			if (percent < 1 || percent > 100)
			{
				throw std::invalid_argument(
					"nearest_rank percent must be in [1, 100].");
			}

			// ceil(percent * count / 100) - 1, using integers throughout.
			return (count * static_cast<std::size_t>(percent) + 99u) / 100u - 1u;
		}

		std::int64_t ranked(const std::vector<std::int64_t>& sorted,
			unsigned int percent)
		{
			return sorted[rank_index(sorted.size(), percent)];
		}
	}

	std::int64_t nearest_rank(const std::vector<std::int64_t>& samples,
		unsigned int percent)
	{
		std::vector<std::int64_t> sorted = samples;
		std::sort(sorted.begin(), sorted.end());
		return ranked(sorted, percent);
	}

	FrameSummary summarize_frames(const std::vector<std::int64_t>& samples)
	{
		if (samples.empty())
		{
			throw std::invalid_argument(
				"summarize_frames requires at least one sample.");
		}

		std::vector<std::int64_t> sorted = samples;
		std::sort(sorted.begin(), sorted.end());

		FrameSummary summary;
		summary.count = sorted.size();
		summary.minimum = sorted.front();
		summary.p50 = ranked(sorted, 50);
		summary.p95 = ranked(sorted, 95);
		summary.p99 = ranked(sorted, 99);
		summary.maximum = sorted.back();
		return summary;
	}
}

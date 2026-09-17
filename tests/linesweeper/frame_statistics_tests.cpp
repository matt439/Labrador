#include "bench/frame_statistics.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

TEST_SUITE("LineSweeper frame statistics")
{
	TEST_CASE("nearest rank returns observed samples without interpolation")
	{
		const std::vector<std::int64_t> samples = { 40, 10, 30, 20 };

		CHECK(bench::nearest_rank(samples, 1) == 10);
		CHECK(bench::nearest_rank(samples, 50) == 20);
		CHECK(bench::nearest_rank(samples, 95) == 40);
		CHECK(bench::nearest_rank(samples, 99) == 40);
		CHECK(bench::nearest_rank(samples, 100) == 40);
	}

	TEST_CASE("summary uses the nearest-rank tail")
	{
		std::vector<std::int64_t> samples;
		for (std::int64_t value = 1; value <= 100; ++value)
		{
			samples.push_back(value);
		}

		const bench::FrameSummary summary = bench::summarize_frames(samples);

		CHECK(summary.count == 100);
		CHECK(summary.minimum == 1);
		CHECK(summary.p50 == 50);
		CHECK(summary.p95 == 95);
		CHECK(summary.p99 == 99);
		CHECK(summary.maximum == 100);
	}

	TEST_CASE("an empty sample and invalid percentiles are refused")
	{
		const std::vector<std::int64_t> empty;
		const std::vector<std::int64_t> one = { 1 };

		CHECK_THROWS_AS(bench::summarize_frames(empty), std::invalid_argument);
		CHECK_THROWS_AS(bench::nearest_rank(empty, 50), std::invalid_argument);
		CHECK_THROWS_AS(bench::nearest_rank(one, 0), std::invalid_argument);
		CHECK_THROWS_AS(bench::nearest_rank(one, 101), std::invalid_argument);
	}
}

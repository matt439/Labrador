#include "bench/bench.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace bench
{
	namespace
	{
		std::vector<Result> all_results;

		// Enough repetitions that the median is not one sample's luck, and few
		// enough that the whole suite stays inside a ctest run somebody is
		// willing to wait for.
		constexpr int REPETITIONS = 9;
		constexpr int WARMUP = 2;

		double median(std::vector<double>& samples)
		{
			std::sort(samples.begin(), samples.end());
			const size_t middle = samples.size() / 2;
			if (samples.size() % 2 == 1)
			{
				return samples[middle];
			}
			return (samples[middle - 1] + samples[middle]) / 2.0;
		}
	}

	Result run(const std::string& name, int64_t n,
		const std::function<void()>& body,
		const std::function<void()>& setup)
	{
		for (int i = 0; i < WARMUP; i++)
		{
			if (setup) { setup(); }
			body();
		}

		std::vector<double> samples;
		samples.reserve(REPETITIONS);

		for (int i = 0; i < REPETITIONS; i++)
		{
			if (setup) { setup(); }

			const auto start = std::chrono::steady_clock::now();
			body();
			const auto end = std::chrono::steady_clock::now();

			samples.push_back(std::chrono::duration<double, std::nano>(
				end - start).count());
		}

		Result result;
		result.name = name;
		result.n = n;
		result.median_ns = median(samples);
		return result;
	}

	const std::vector<Result>& results()
	{
		return all_results;
	}

	void record(const Result& result)
	{
		all_results.push_back(result);
	}

	void report()
	{
		std::printf("\n%-34s %10s %14s %12s\n",
			"case", "objects", "median (us)", "ns/object");
		std::printf("%-34s %10s %14s %12s\n",
			"----------------------------------", "----------",
			"--------------", "------------");

		std::string previous;
		for (const Result& r : all_results)
		{
			if (!previous.empty() && r.name != previous)
			{
				std::printf("\n");
			}
			previous = r.name;

			std::printf("%-34s %10lld %14.1f %12.1f\n",
				r.name.c_str(),
				static_cast<long long>(r.n),
				r.median_ns / 1000.0,
				r.ns_per_n());
		}
		std::printf("\n");
	}

	std::string check_scaling(const Result& small, const Result& large,
		double allowed_growth)
	{
		const double small_cost = small.ns_per_n();
		const double large_cost = large.ns_per_n();

		if (small_cost <= 0.0)
		{
			return {};
		}

		const double growth = large_cost / small_cost;
		if (growth <= allowed_growth)
		{
			return {};
		}

		char message[512];
		std::snprintf(message, sizeof(message),
			"%s: per-object cost grew %.2fx between n=%lld (%.1f ns/object) "
			"and n=%lld (%.1f ns/object), allowed %.2fx. That is a change of "
			"complexity class, not a slow machine.",
			small.name.c_str(), growth,
			static_cast<long long>(small.n), small_cost,
			static_cast<long long>(large.n), large_cost,
			allowed_growth);
		return message;
	}
}

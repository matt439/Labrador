#include "bench/bench.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

void run_scene_benchmarks();

namespace
{
	// How much per-object cost a phase is allowed to gain between the smallest
	// and largest run before it is called a change of complexity class.
	//
	// Generous, and deliberately so. Between n=64 and n=4096 a linear phase
	// gains real cost from cache behaviour alone - the working set crosses L1
	// and then L2 - so a tight bound here would fail on a busy machine and
	// teach everyone to ignore it. 8x over a 64x growth in n still catches the
	// thing this is for: a phase that quietly became quadratic would show 64x.
	constexpr double LINEAR_SLACK = 8.0;

	// The phases that must stay linear in the object count.
	const char* const LINEAR_PHASES[] = {
		"Scene::update",
		"Scene::end_tick",
		"render cull (one view)",
		// The grid is why this one is here. Before it, resolve was the
		// all-pairs sweep and could not have been on this list.
		"Scene::resolve (broad phase)",
	};

	std::vector<bench::Result> for_case(const std::string& name)
	{
		std::vector<bench::Result> out;
		for (const bench::Result& r : bench::results())
		{
			if (r.name == name)
			{
				out.push_back(r);
			}
		}
		return out;
	}
}

int main()
{
	run_scene_benchmarks();
	bench::report();

	int failures = 0;
	for (const char* phase : LINEAR_PHASES)
	{
		const std::vector<bench::Result> runs = for_case(phase);
		if (runs.size() < 2)
		{
			continue;
		}

		const std::string failure = bench::check_scaling(runs.front(),
			runs.back(), LINEAR_SLACK);
		if (!failure.empty())
		{
			std::printf("FAILED  %s\n", failure.c_str());
			failures++;
		}
		else
		{
			std::printf("ok      %s stays linear in the object count\n", phase);
		}
	}

	// The sweep the grid replaced, kept as the thing to compare against and as
	// the reference implementation the grid is checked for equality with
	// (tests/collision/broad_phase_tests.cpp). It is quadratic by
	// construction, so it is reported rather than asserted on.
	const std::vector<bench::Result> sweep =
		for_case("Scene::resolve (n^2 sweep)");
	const std::vector<bench::Result> indexed =
		for_case("Scene::resolve (broad phase)");

	if (sweep.size() >= 2)
	{
		const double growth = sweep.back().ns_per_n() / sweep.front().ns_per_n();
		std::printf("note    the all-pairs sweep is O(n^2): per-object cost "
			"grew %.1fx between n=%lld and n=%lld, against %.1fx growth in n\n",
			growth,
			static_cast<long long>(sweep.front().n),
			static_cast<long long>(sweep.back().n),
			static_cast<double>(sweep.back().n) /
				static_cast<double>(sweep.front().n));
	}

	if (sweep.size() == indexed.size())
	{
		for (size_t i = 0; i < sweep.size(); i++)
		{
			std::printf("note    n=%-5lld the broad phase is %6.1fx faster "
				"than the sweep\n",
				static_cast<long long>(sweep[i].n),
				sweep[i].median_ns / indexed[i].median_ns);
		}
	}

	if (failures > 0)
	{
		std::printf("\n%d benchmark(s) changed complexity class.\n", failures);
		return 1;
	}

	std::printf("\nAll benchmarks kept their complexity class.\n");
	return 0;
}

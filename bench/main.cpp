#include "bench/bench.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

void run_scene_benchmarks();
void run_render_benchmarks();

// Compiled from fanout_bench_null.cpp against the null backend and from
// fanout_bench_absent.cpp against the other four - bench/CMakeLists.txt lists
// one or the other, so nothing here knows which backend it was built against.
// The returned string is a note for under the table: what the fan-out did on
// this run, or where to go to see it do anything at all.
std::string run_fanout_benchmarks();

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

		// The draw path's engine-side arithmetic (render_bench.cpp). Linear
		// by construction - four corners a sprite, no lookup and no
		// allocation - so an accidental O(n^2) is not what these are
		// guarding. What they guard is the change that would make a sprite's
		// cost depend on how many sprites there are, which is exactly the
		// shape a bulk or instanced submission path has if it sorts or
		// groups; docs/survey/2026-08-26.md 5 names that as the item this
		// benchmark exists to let somebody argue.
		"build_sprite_quad",
		"build_sprite_quad (rotated)",
		"build_scaled_quad",

		// The per-view render fan-out, which exists in the null configuration
		// alone (fanout_bench_null.cpp). Both rows are linear for the reason
		// the cull is - a bounds test and at most a draw, per object per view,
		// and nothing that looks anything up - so what they guard is what
		// every other phase here guards: a change that makes one object's cost
		// depend on how many objects there are. How much the fan-out actually
		// won is a note below and not an assertion, because that is wall-clock
		// and a property of the machine's core count.
		"Scene::draw (4 views, serial)",
		"Scene::draw (4 views, fan-out)",
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
	run_render_benchmarks();
	const std::string fanout_note = run_fanout_benchmarks();
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

	// What the angle branch inside build_quad buys, which is the one claim
	// engine/render/sprite_geometry.cpp makes about its own speed: "a sine and
	// a cosine per sprite for an angle of zero is a real cost in a frame that
	// draws thousands". Reported rather than asserted on - the ratio is a
	// property of this machine's transcendentals, and the branch stays worth
	// having at any ratio above one - but printed, because until it was
	// printed the claim was an assertion in a comment.
	const std::vector<bench::Result> upright = for_case("build_sprite_quad");
	const std::vector<bench::Result> turned =
		for_case("build_sprite_quad (rotated)");

	if (upright.size() == turned.size())
	{
		for (size_t i = 0; i < upright.size(); i++)
		{
			if (upright[i].ns_per_n() <= 0.0)
			{
				continue;
			}

			std::printf("note    n=%-5lld a rotated sprite costs %5.2fx an "
				"upright one (%.2f against %.2f ns/sprite)\n",
				static_cast<long long>(upright[i].n),
				turned[i].ns_per_n() / upright[i].ns_per_n(),
				turned[i].ns_per_n(),
				upright[i].ns_per_n());
		}
	}

	// What the fan-out bought, at each count, on this machine. Reported for the
	// reason the broad phase's ratio is: it is a property of this box's core
	// count and of this backend's idea of a draw, where bench.h asserts on
	// complexity class alone.
	const std::vector<bench::Result> one_thread =
		for_case("Scene::draw (4 views, serial)");
	const std::vector<bench::Result> fanned =
		for_case("Scene::draw (4 views, fan-out)");

	if (one_thread.size() == fanned.size())
	{
		for (size_t i = 0; i < one_thread.size(); i++)
		{
			if (fanned[i].median_ns <= 0.0)
			{
				continue;
			}

			std::printf("note    n=%-5lld the fan-out draws four views %5.2fx "
				"the speed of one thread\n",
				static_cast<long long>(one_thread[i].n),
				one_thread[i].median_ns / fanned[i].median_ns);
		}
	}

	std::printf("%s\n", fanout_note.c_str());

	if (failures > 0)
	{
		std::printf("\n%d benchmark(s) changed complexity class.\n", failures);
		return 1;
	}

	std::printf("\nAll benchmarks kept their complexity class.\n");
	return 0;
}

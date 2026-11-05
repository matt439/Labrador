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

// Whether run_fanout_benchmarks recorded the two Scene::draw rows: true from
// fanout_bench_null.cpp, false from fanout_bench_absent.cpp. Only those two
// phases may be absent, and only from a build that does not measure them.
bool fanout_measured();

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
	constexpr double linear_slack = 8.0;

	// A phase that must stay linear in the object count. The names are
	// matched against the names passed to bench::run in the other files, so
	// a phase with fewer than two results fails rather than passing unseen:
	// a rename on either side would otherwise turn its assertion off. `fanout`
	// marks the two rows only fanout_bench_null.cpp records, which alone may
	// be missing, and only from a build that does not measure them.
	struct LinearPhase
	{
		const char* name = nullptr;
		bool fanout = false;
	};

	const LinearPhase linear_phases[] = {
		{ "Scene::update", false },
		{ "Scene::end_tick", false },
		{ "render cull (one view)", false },
		// Linear because of the broad phase's grid; an all-pairs
		// enumeration would be quadratic.
		{ "Scene::resolve (broad phase)", false },

		// The draw path's engine-side arithmetic (render_bench.cpp). Linear
		// by construction - four corners a sprite, no lookup and no
		// allocation - so an accidental O(n^2) is not what these are
		// guarding. What they guard is the change that would make a sprite's
		// cost depend on how many sprites there are, which is exactly the
		// shape a bulk or instanced submission path has if it sorts or
		// groups; docs/survey/2026-08-26.md 5 names that as the item this
		// benchmark exists to let somebody argue.
		{ "build_sprite_quad", false },
		{ "build_sprite_quad (rotated)", false },
		{ "build_scaled_quad", false },

		// The per-view render fan-out, which exists in the null configuration
		// alone (fanout_bench_null.cpp). Both rows are linear for the reason
		// the cull is - a bounds test and at most a draw, per object per view,
		// and nothing that looks anything up - so what they guard is what
		// every other phase here guards: a change that makes one object's cost
		// depend on how many objects there are. How much the fan-out actually
		// won is a note below and not an assertion, because that is wall-clock
		// and a property of the machine's core count.
		{ "Scene::draw (4 views, serial)", true },
		{ "Scene::draw (4 views, fan-out)", true },
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

	// Two cases' results at the same n, for the notes that print one as a
	// multiple of the other.
	struct ResultPair
	{
		bench::Result over;
		bench::Result under;

		double ratio() const
		{
			return this->over.median_ns / this->under.median_ns;
		}
	};

	// Pairs by n rather than by position, and leaves out a pair whose
	// denominator measured nothing: that ratio would be infinite and say
	// nothing about either case.
	std::vector<ResultPair> paired_by_n(const std::vector<bench::Result>& over,
		const std::vector<bench::Result>& under)
	{
		std::vector<ResultPair> out;
		for (const bench::Result& top : over)
		{
			for (const bench::Result& bottom : under)
			{
				if (top.n == bottom.n && bottom.median_ns > 0.0)
				{
					out.push_back({ top, bottom });
				}
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
	const bool fanout = fanout_measured();
	for (const LinearPhase& phase : linear_phases)
	{
		if (phase.fanout && !fanout)
		{
			continue;
		}

		const std::vector<bench::Result> runs = for_case(phase.name);
		if (runs.size() < 2)
		{
			std::printf("FAILED  %s: recorded %d result(s) where its complexity "
				"class needs two. A case was renamed or not run.\n",
				phase.name, static_cast<int>(runs.size()));
			failures++;
			continue;
		}

		const std::string failure = bench::check_scaling(runs.front(),
			runs.back(), linear_slack);
		if (!failure.empty())
		{
			std::printf("FAILED  %s\n", failure.c_str());
			failures++;
		}
		else
		{
			std::printf("ok      %s stays linear in the object count\n",
				phase.name);
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

	if (sweep.size() >= 2 && sweep.front().ns_per_n() > 0.0)
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

	for (const ResultPair& pair : paired_by_n(sweep, indexed))
	{
		std::printf("note    n=%-5lld the broad phase is %6.1fx faster "
			"than the sweep\n",
			static_cast<long long>(pair.over.n), pair.ratio());
	}

	// What the angle branch inside build_quad buys, which is the one claim
	// engine/render/sprite_geometry.cpp makes about its own speed: "a sine and
	// a cosine per sprite for an angle of zero is a real cost in a frame that
	// draws thousands". Reported rather than asserted on - the ratio is a
	// property of this machine's transcendentals, and the branch stays worth
	// having at any ratio above one - but printed, so the claim is measured
	// rather than only asserted in a comment.
	const std::vector<bench::Result> upright = for_case("build_sprite_quad");
	const std::vector<bench::Result> turned =
		for_case("build_sprite_quad (rotated)");

	for (const ResultPair& pair : paired_by_n(turned, upright))
	{
		std::printf("note    n=%-5lld a rotated sprite costs %5.2fx an "
			"upright one (%.2f against %.2f ns/sprite)\n",
			static_cast<long long>(pair.over.n), pair.ratio(),
			pair.over.ns_per_n(), pair.under.ns_per_n());
	}

	// What the fan-out bought, at each count, on this machine. Reported for the
	// reason the broad phase's ratio is: it is a property of this box's core
	// count and of this backend's idea of a draw, where bench.h asserts on
	// complexity class alone.
	const std::vector<bench::Result> one_thread =
		for_case("Scene::draw (4 views, serial)");
	const std::vector<bench::Result> fanned =
		for_case("Scene::draw (4 views, fan-out)");

	for (const ResultPair& pair : paired_by_n(one_thread, fanned))
	{
		std::printf("note    n=%-5lld the fan-out draws four views %5.2fx "
			"the speed of one thread\n",
			static_cast<long long>(pair.over.n), pair.ratio());
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

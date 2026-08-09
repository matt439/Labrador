#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The benchmark harness PHILOSOPHY's Performance section asks for.
//
// "Benchmarks pin throughput the way tests pin behaviour: representative
// scenes with large object counts, run alongside the test suite. A throughput
// regression is a defect, not a curiosity." The word "benchmark" appeared zero
// times in 863 review findings, and there was no target to put one in.
//
// WHAT THIS ASSERTS, AND WHAT IT ONLY REPORTS. Absolute timings are a property
// of the machine, so pinning them would either fail on a slower box or pass on
// a faster one after a real regression. What is machine-independent is the
// *shape* of the curve: a phase that is linear in the object count stays
// linear when the count quadruples, whatever the constant. So the numbers are
// reported for a human and for a CI log, and the assertions are on complexity
// class - which is the thing an accidental O(n^2) actually breaks.
//
// It is deliberately not a third-party benchmark library. What is needed is a
// steady clock, a warm-up and a median, and adding a dependency to vcpkg.json
// for that would be the speculative framework T1 rules out.
namespace bench
{
	// One measured case at one problem size.
	struct Result
	{
		std::string name;
		int64_t n = 0;

		// Nanoseconds for one iteration of the whole case, median over the
		// repetitions. Median rather than mean because a scheduler hiccup on a
		// developer machine is an outlier, not a signal.
		double median_ns = 0.0;

		// The cost the complexity assertions are made against.
		double ns_per_n() const
		{
			return this->n > 0 ? this->median_ns / static_cast<double>(this->n)
				: this->median_ns;
		}
	};

	// Runs `body` until the timings settle, and returns the median.
	//
	// `setup` runs before each timed repetition and is not counted, which is
	// what lets a case that mutates its input measure the same work every
	// time.
	Result run(const std::string& name, int64_t n,
		const std::function<void()>& body,
		const std::function<void()>& setup = {});

	// Every result the process has produced, in the order it produced them.
	const std::vector<Result>& results();
	void record(const Result& result);

	// Prints the table. One line per case, grouped by name.
	void report();

	// Whether the per-object cost grew by more than `factor` between two
	// results of the same case. `growth` of 1.0 would be perfectly linear;
	// anything set here is the slack allowed for cache effects and for a busy
	// machine.
	//
	// Returns an empty string when the check passes, and the failure sentence
	// when it does not, so the caller decides whether that is fatal.
	std::string check_scaling(const Result& small, const Result& large,
		double allowed_growth);
}

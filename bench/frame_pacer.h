#pragma once

#include <chrono>

namespace bench
{
	// Benchmark-only Windows pacing. A high-resolution timer avoids rounding
	// each sleep to milliseconds, without spinning or changing timer resolution
	// for the process. The scheduler can still wake us late; callers retain it.
	class FramePacer final
	{
	public:
		FramePacer();
		~FramePacer();
		FramePacer(const FramePacer&) = delete;
		FramePacer& operator=(const FramePacer&) = delete;

		// Returns the first observed time at or after the deadline. An expired
		// deadline returns immediately: the caller owns the catch-up policy.
		std::chrono::steady_clock::time_point wait_until(
			std::chrono::steady_clock::time_point deadline) const;

	private:
		void* timer_ = nullptr;
	};
}

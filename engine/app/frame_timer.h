#pragma once

#include <chrono>

namespace labrador
{
	struct FrameTimings;

	// The marks one rendered tick leaves, after the one it starts with
	// (FrameTimer::begin). Each is the moment the phase it ends had finished.
	struct FrameMarks
	{
		int updates = 0;
		std::chrono::steady_clock::time_point updated;
		std::chrono::steady_clock::time_point frame_begun;
		std::chrono::steady_clock::time_point drawn;
		std::chrono::steady_clock::time_point submitted;
		std::chrono::steady_clock::time_point frame_ended;
	};

	// Turns the time points Application takes in tick() and render() into the
	// FrameTimings it publishes (application.h states the contract).
	//
	// IT READS NO CLOCK. Every time point is an argument, so the arithmetic is
	// tested with points a test chose, and the number of clock reads per frame
	// is fixed by the caller rather than hidden here - which matters in a
	// browser, where each one is a call out to performance.now().
	class FrameTimer
	{
	public:
		// A tick began. Its frame is published only if publish() follows; a
		// tick that throws, or renders nothing, leaves the last snapshot as it
		// was.
		void begin(std::chrono::steady_clock::time_point tick_start);

		// The time since the last frame was not the game's: a reset clock, or
		// coming back from being hidden. The next published interval is zero,
		// and so is the one after it when this arrives during a tick, because
		// that tick's start is then no baseline for the next.
		void discontinuity();

		// The tick begun last rendered a frame, which these marks describe.
		// Writes it into `timings` as the next frame.
		void publish(const FrameMarks& marks, FrameTimings& timings);

	private:
		std::chrono::steady_clock::time_point tick_start_;
		std::chrono::steady_clock::time_point previous_start_;
		bool has_previous_ = false;
		bool start_is_baseline_ = true;
	};
}

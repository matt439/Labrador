#include "engine/app/frame_timer.h"

#include "engine/app/application.h"

#include <chrono>
#include <cstdint>

namespace labrador
{
	void FrameTimer::begin(std::chrono::steady_clock::time_point tick_start)
	{
		this->tick_start_ = tick_start;
		this->start_is_baseline_ = true;
	}

	void FrameTimer::discontinuity()
	{
		this->has_previous_ = false;
		this->start_is_baseline_ = false;
	}

	void FrameTimer::publish(const FrameMarks& marks, FrameTimings& timings)
	{
		FrameTimings frame;
		frame.frame = timings.frame + 1;
		frame.updates = marks.updates;
		if (this->has_previous_)
		{
			frame.interval = this->tick_start_ - this->previous_start_;
		}
		frame.update_time = marks.updated - this->tick_start_;
		frame.begin_frame_time = marks.frame_begun - marks.updated;
		frame.draw_time = marks.drawn - marks.frame_begun;
		frame.submit_time = marks.submitted - marks.drawn;
		frame.end_frame_time = marks.frame_ended - marks.submitted;

		// A start a discontinuity arrived after is no baseline: the time
		// between it and the next tick holds whatever was not the game's.
		this->has_previous_ = this->start_is_baseline_;
		this->previous_start_ = this->tick_start_;

		timings = frame;
	}

	std::chrono::nanoseconds minimized_wait(std::uint64_t left_over_ticks,
		std::uint64_t step_ticks)
	{
		if (step_ticks == 0 || left_over_ticks >= step_ticks)
		{
			return std::chrono::nanoseconds::zero();
		}

		// Compared in ticks, before the multiplication, so a step of any
		// length reaches the cap without overflowing on the way.
		const std::uint64_t ticks_per_second = StepTimer::TicksPerSecond;
		const std::uint64_t longest_ticks = ticks_per_second / 1000 *
			static_cast<std::uint64_t>(longest_step_wait.count());
		const std::uint64_t remaining = step_ticks - left_over_ticks;
		if (remaining >= longest_ticks)
		{
			return longest_step_wait;
		}

		const std::uint64_t nanoseconds_per_tick =
			1000000000 / ticks_per_second;
		return std::chrono::nanoseconds(
			static_cast<std::chrono::nanoseconds::rep>(
				remaining * nanoseconds_per_tick));
	}
}

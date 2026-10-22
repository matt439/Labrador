#include "engine/app/frame_timer.h"

#include "engine/app/application.h"

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
}

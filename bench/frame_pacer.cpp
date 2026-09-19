#include "bench/frame_pacer.h"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace bench
{
	FramePacer::FramePacer()
	{
		this->timer_ = CreateWaitableTimerExW(nullptr, nullptr,
			CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
		if (this->timer_ == nullptr)
		{
			throw std::runtime_error(
				"The frame benchmark requires a high-resolution waitable timer (Windows 10 1803 or later).");
		}
	}

	FramePacer::~FramePacer()
	{
		CloseHandle(this->timer_);
	}

	std::chrono::steady_clock::time_point FramePacer::wait_until(
		std::chrono::steady_clock::time_point deadline) const
	{
		using Clock = std::chrono::steady_clock;
		Clock::time_point now = Clock::now();
		while (now < deadline)
		{
			// Negative due times are relative, in 100 ns units. Round up so a
			// sub-tick remainder cannot become zero (an absolute, expired time).
			const std::int64_t remaining =
				std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
			LARGE_INTEGER due = {};
			due.QuadPart = -(remaining / 100 + 1);
			if (!SetWaitableTimerEx(this->timer_, &due, 0, nullptr, nullptr, nullptr, 0) ||
				WaitForSingleObject(this->timer_, INFINITE) != WAIT_OBJECT_0)
			{
				throw std::runtime_error("The frame benchmark's high-resolution wait failed.");
			}
			now = Clock::now();
		}
		return now;
	}
}

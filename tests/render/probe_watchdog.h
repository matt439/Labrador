#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace render_tests
{
	// Bounds an opt-in probe process. If the watchdog is still alive when its
	// limit expires, or is destroyed after that, it calls `expire` with the
	// reason; `expire` reports in the probe's own words and ends the process
	// without returning, and the process ends regardless if it does return.
	// Declared first in main, so it outlives every engine owner the probe
	// makes and covers their teardown too.
	class ProbeWatchdog
	{
	public:
		using Expiry = void (*)(const char* reason) noexcept;

		ProbeWatchdog(std::chrono::seconds limit, Expiry expire)
			: expire_(expire),
			deadline_(std::chrono::steady_clock::now() + limit),
			worker_([this]() { this->watch(); })
		{
		}

		~ProbeWatchdog()
		{
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				this->check_deadline();
				this->finished_ = true;
			}
			this->condition_.notify_one();
			this->worker_.join();
			this->check_deadline();
		}

		ProbeWatchdog(const ProbeWatchdog&) = delete;
		ProbeWatchdog& operator=(const ProbeWatchdog&) = delete;

	private:
		void watch() noexcept
		{
			std::unique_lock<std::mutex> lock(this->mutex_);
			if (!this->condition_.wait_until(lock, this->deadline_,
				[this]() { return this->finished_; }))
			{
				this->expire("process deadline expired");
			}
		}

		void check_deadline() const noexcept
		{
			if (std::chrono::steady_clock::now() >= this->deadline_)
			{
				this->expire("completion deadline expired");
			}
		}

		[[noreturn]] void expire(const char* reason) const noexcept
		{
			this->expire_(reason);
			std::_Exit(EXIT_FAILURE);
		}

		// Initialised in this order, so the worker starts last.
		const Expiry expire_;
		const std::chrono::steady_clock::time_point deadline_;
		std::mutex mutex_;
		std::condition_variable condition_;
		bool finished_ = false;
		std::thread worker_;
	};
}

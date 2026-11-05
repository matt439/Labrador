#pragma once

#include <chrono>

namespace labrador::detail
{
	// DirectXTK clears its critical-error flag even when Reset finds no output.
	// Retry silent mode at a bounded rate, including a launch without a device.
	class DeviceRecovery
	{
	public:
		using Clock = std::chrono::steady_clock;

		template<typename Engine>
		void update(Engine& engine, Clock::time_point now)
		{
			if (this->suspended_)
			{
				return;
			}
			if (engine.Update())
			{
				this->next_retry_ = Clock::time_point::min();
				return;
			}
			if (now < this->next_retry_)
			{
				return;
			}

			this->next_retry_ = now + std::chrono::seconds(1);
			if (engine.Reset())
			{
				this->next_retry_ = Clock::time_point::min();
			}
		}

		void suspend() noexcept
		{
			this->suspended_ = true;
		}

		void resume() noexcept
		{
			this->suspended_ = false;
			this->next_retry_ = Clock::time_point::min();
		}

		// Between suspend() and resume(), which is also when a one-shot is
		// dropped rather than started on a stopped engine (audio_device.h).
		bool suspended() const noexcept
		{
			return this->suspended_;
		}

	private:
		Clock::time_point next_retry_ = Clock::time_point::min();
		bool suspended_ = false;
	};
}

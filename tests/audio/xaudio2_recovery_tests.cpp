#include <doctest/doctest.h>

#include "engine/audio/xaudio2/device_recovery.h"

#include <chrono>
#include <stdexcept>

using namespace labrador;

namespace
{
	// Models DirectXTK's silent/reset answers without creating an audio graph.
	// These cases check retry policy, not endpoint recovery or audible output.
	struct RecoveryEngine
	{
		bool output = true;
		bool reset_output = false;
		bool reset_throws = false;
		bool critical_error = false;
		int updates = 0;
		int resets = 0;

		bool Update()
		{
			++this->updates;
			return this->output;
		}

		bool Reset()
		{
			++this->resets;
			// The pinned DirectXTK clears this even when no endpoint exists.
			this->critical_error = false;
			if (this->reset_throws)
			{
				throw std::runtime_error("reset failure");
			}
			this->output = this->reset_output;
			return this->output;
		}
	};

	using Clock = detail::DeviceRecovery::Clock;
	const Clock::time_point start = Clock::time_point{};
}

TEST_CASE("XAudio2 recovery leaves healthy output alone")
{
	detail::DeviceRecovery recovery;
	RecoveryEngine engine;
	recovery.update(engine, start);
	recovery.update(engine, start + std::chrono::seconds(2));
	CHECK(engine.updates == 2);
	CHECK(engine.resets == 0);
}

TEST_CASE("XAudio2 retries initially silent output at one second intervals")
{
	detail::DeviceRecovery recovery;
	RecoveryEngine engine;
	engine.output = false;
	CHECK_FALSE(engine.critical_error);
	recovery.update(engine, start);
	CHECK(engine.resets == 1);
	engine.reset_output = true;
	recovery.update(engine, start + std::chrono::milliseconds(999));
	CHECK(engine.resets == 1);
	CHECK_FALSE(engine.output);
	recovery.update(engine, start + std::chrono::seconds(1));
	CHECK(engine.resets == 2);
	CHECK(engine.output);
	recovery.update(engine, start + std::chrono::seconds(2));
	CHECK(engine.resets == 2);
}

TEST_CASE("XAudio2 keeps retrying after reset clears the critical error")
{
	detail::DeviceRecovery recovery;
	RecoveryEngine engine;
	engine.output = false;
	engine.critical_error = true;
	recovery.update(engine, start);
	CHECK_FALSE(engine.critical_error);
	CHECK(engine.resets == 1);
	recovery.update(engine, start + std::chrono::seconds(1));
	CHECK(engine.resets == 2);
	engine.reset_output = true;
	recovery.update(engine, start + std::chrono::seconds(2));
	REQUIRE(engine.output);
	// A fresh loss after success does not inherit the previous retry delay.
	engine.output = false;
	recovery.update(engine, start + std::chrono::milliseconds(2001));
	CHECK(engine.resets == 4);
}

TEST_CASE("XAudio2 defers silent recovery while suspended and retries on resume")
{
	detail::DeviceRecovery recovery;
	RecoveryEngine engine;
	engine.output = false;
	recovery.update(engine, start);
	std::chrono::milliseconds suspended_time(200);
	SUBCASE("resume before the failed-reset deadline") {}
	SUBCASE("remain suspended past the failed-reset deadline")
	{
		suspended_time = std::chrono::milliseconds(2000);
	}
	recovery.suspend();
	recovery.update(engine, start + suspended_time);
	CHECK(engine.updates == 1);
	CHECK(engine.resets == 1);
	recovery.resume();
	engine.reset_output = true;
	recovery.update(engine, start + suspended_time + std::chrono::milliseconds(1));
	CHECK(engine.resets == 2);
	CHECK(engine.output);
}

TEST_CASE("XAudio2 resume can recover output that became silent without a critical error")
{
	detail::DeviceRecovery recovery;
	RecoveryEngine engine;
	recovery.update(engine, start);
	recovery.suspend();
	engine.output = false;
	engine.reset_output = true;
	recovery.resume();
	recovery.update(engine, start + std::chrono::milliseconds(1));
	CHECK(engine.resets == 1);
	CHECK(engine.output);
}

TEST_CASE("XAudio2 reset exceptions propagate and retain the retry interval")
{
	detail::DeviceRecovery recovery;
	RecoveryEngine engine;
	engine.output = false;
	engine.reset_throws = true;
	CHECK_THROWS_WITH_AS(recovery.update(engine, start), "reset failure", std::runtime_error);
	engine.reset_throws = false;
	engine.reset_output = true;
	recovery.update(engine, start + std::chrono::milliseconds(999));
	CHECK(engine.resets == 1);
	recovery.update(engine, start + std::chrono::seconds(1));
	CHECK(engine.resets == 2);
	CHECK(engine.output);
}

#include <doctest/doctest.h>

#include "engine/audio/xaudio2/device_recovery.h"
#include "engine/audio/xaudio2/wave_bank_policy.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

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

namespace
{
	// Stands in for WAVEFORMATEX, which this file does not include.
	struct PolicyFormat
	{
		unsigned short tag = 0;
	};

	// Models the members of DirectXTK's WaveBank that wave_bank_policy.h
	// calls, answering as the pinned source does: nothing past the end.
	struct PolicyBank
	{
		// Each entry's size in bytes; an empty entry is held and has none.
		std::vector<std::size_t> sizes;
		bool streaming = false;

		struct Played
		{
			unsigned int index = 0;
			float volume = 0.0f;
		};
		std::vector<Played> played;

		std::size_t GetSampleSizeInBytes(unsigned int index) const noexcept
		{
			return index < this->sizes.size() ? this->sizes[index] : 0;
		}

		const PolicyFormat* GetFormat(unsigned int index, PolicyFormat* format,
			std::size_t maxsize) const noexcept
		{
			if (index >= this->sizes.size() || maxsize < sizeof(PolicyFormat))
			{
				return nullptr;
			}
			return format;
		}

		bool IsStreamingBank() const noexcept
		{
			return this->streaming;
		}

		void Play(unsigned int index, float volume, float, float)
		{
			this->played.push_back(Played{ index, volume });
		}
	};

	// Constructs a PolicyBank or fails as a reader would. The failure is
	// read through volatile so no build sees an opener that always throws
	// and folds away the policy code after the call.
	struct PolicyOpener
	{
		enum class Failure
		{
			none,
			reader,
			missing,
			memory
		};

		volatile Failure failure = Failure::none;
		bool streaming = false;

		std::unique_ptr<PolicyBank> operator()() const
		{
			const Failure failure_now = this->failure;
			if (failure_now == Failure::reader)
			{
				throw std::runtime_error("WaveBank");
			}
			if (failure_now == Failure::missing)
			{
				throw std::out_of_range("no file");
			}
			if (failure_now == Failure::memory)
			{
				throw std::bad_alloc();
			}

			std::unique_ptr<PolicyBank> bank = std::make_unique<PolicyBank>();
			bank->streaming = this->streaming;
			return bank;
		}
	};
}

TEST_CASE("XAudio2 holds a wave index only inside the bank")
{
	PolicyBank bank;
	bank.sizes = { 64, 0, 128 };

	CHECK(detail::holds_wave<PolicyFormat>(bank, 0));
	CHECK(detail::holds_wave<PolicyFormat>(bank, 1));      // empty, still held
	CHECK(detail::holds_wave<PolicyFormat>(bank, 2));
	CHECK_FALSE(detail::holds_wave<PolicyFormat>(bank, 3));
	CHECK_FALSE(detail::holds_wave<PolicyFormat>(bank, -1));
}

TEST_CASE("XAudio2 plays a held one-shot, refuses another bank's index, and drops while suspended")
{
	PolicyBank bank;
	bank.sizes = { 64, 64 };
	detail::DeviceRecovery recovery;

	detail::play_one_shot<PolicyFormat>(bank, recovery, 1, 0.5f, 0.0f, 0.0f);
	REQUIRE(bank.played.size() == 1);
	CHECK(bank.played[0].index == 1);
	CHECK(bank.played[0].volume == doctest::Approx(0.5f));

	// The throw null and Web Audio make, where DirectXTK's Play is silent.
	CHECK_THROWS_WITH_AS(detail::play_one_shot<PolicyFormat>(bank, recovery,
		2, 1.0f, 0.0f, 0.0f), "A wave was played that the bank does not hold.",
		std::out_of_range);
	CHECK_THROWS_AS(detail::play_one_shot<PolicyFormat>(bank, recovery,
		-1, 1.0f, 0.0f, 0.0f), std::out_of_range);

	recovery.suspend();
	CHECK(recovery.suspended());
	detail::play_one_shot<PolicyFormat>(bank, recovery, 0, 1.0f, 0.0f, 0.0f);
	CHECK(bank.played.size() == 1);

	// Suspended or not, the index is checked first.
	CHECK_THROWS_AS(detail::play_one_shot<PolicyFormat>(bank, recovery,
		2, 1.0f, 0.0f, 0.0f), std::out_of_range);

	recovery.resume();
	CHECK_FALSE(recovery.suspended());
	detail::play_one_shot<PolicyFormat>(bank, recovery, 0, 1.0f, 0.0f, 0.0f);
	CHECK(bank.played.size() == 2);
}

TEST_CASE("XAudio2 reports a failed bank open naming the file and the reader's words")
{
	const std::string path = "./sounds/effects.xwb";
	PolicyOpener opener;

	SUBCASE("a reader failure is the runtime_error an optional bank is substituted for")
	{
		opener.failure = PolicyOpener::Failure::reader;
		try
		{
			std::ignore = detail::open_wave_bank<PolicyBank>(path, opener);
			FAIL("the failed open was not reported");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message(error.what());
			CHECK(message.find("Failed to open wave bank: " + path) !=
				std::string::npos);
			CHECK(message.find("(WaveBank)") != std::string::npos);
		}
	}

	SUBCASE("the reader's out_of_range is converted too")
	{
		opener.failure = PolicyOpener::Failure::missing;
		CHECK_THROWS_WITH_AS(std::ignore =
			detail::open_wave_bank<PolicyBank>(path, opener),
			doctest::Contains("(no file)"), std::runtime_error);
	}

	SUBCASE("running out of memory is not a missing bank")
	{
		opener.failure = PolicyOpener::Failure::memory;
		CHECK_THROWS_AS(std::ignore =
			detail::open_wave_bank<PolicyBank>(path, opener), std::bad_alloc);
	}

	SUBCASE("a streaming bank is refused at load, naming the file")
	{
		opener.streaming = true;
		CHECK_THROWS_WITH_AS(std::ignore =
			detail::open_wave_bank<PolicyBank>(path, opener),
			doctest::Contains("./sounds/effects.xwb is a streaming bank"),
			std::runtime_error);
	}

	SUBCASE("an in-memory bank is handed back")
	{
		const std::unique_ptr<PolicyBank> bank =
			detail::open_wave_bank<PolicyBank>(path, opener);
		CHECK(bank != nullptr);
	}
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

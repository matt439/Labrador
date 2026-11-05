#include <doctest/doctest.h>

#include "engine/audio/audio_device.h"
#include "engine/audio/wave_tables.h"

#include <stdexcept>
#include <string>
#include <vector>

using namespace labrador;

// The tables the null and Web Audio backends share, asserted on every backend:
// the header is engine code that no API reaches, so the refusals both of them
// make are pinned here once rather than only through whichever one a preset
// builds.

TEST_CASE("wave tables answer each bank's indices in the order it was given")
{
	WaveTables tables;
	const AudioDevice::WaveBankHandle first =
		tables.add_bank({ "ping", "chime" });
	const AudioDevice::WaveBankHandle second = tables.add_bank({ "drone" });

	CHECK(first.index() == 0);
	CHECK(second.index() == 1);
	CHECK(tables.wave_index(first, "chime") == 1);
	CHECK(tables.wave_index(second, "drone") == 0);
	CHECK(tables.wave_index(second, "ping") == -1);
	const std::vector<std::string> expected = { "ping", "chime" };
	CHECK(tables.bank(first) == expected);
}

TEST_CASE("wave tables refuse unresolved handles and waves a bank lacks")
{
	WaveTables tables;
	const AudioDevice::WaveBankHandle bank = tables.add_bank({ "ping" });

	CHECK_THROWS_AS(tables.wave_index(AudioDevice::WaveBankHandle(), "ping"),
		std::out_of_range);
	CHECK_THROWS_AS(tables.bank(AudioDevice::WaveBankHandle(1)),
		std::out_of_range);
	CHECK_NOTHROW(tables.check_voice_wave(bank, 0));
	CHECK_THROWS_WITH_AS(tables.check_voice_wave(bank, 1),
		"A voice was asked for a wave the bank does not hold.",
		std::out_of_range);
	CHECK_THROWS_WITH_AS(tables.check_played_wave(bank, -1),
		"A wave was played that the bank does not hold.",
		std::out_of_range);
	CHECK_THROWS_WITH_AS(tables.voice(AudioDevice::VoiceHandle(0)),
		"A voice was reached through an unresolved handle.",
		std::out_of_range);
}

TEST_CASE("wave tables keep each voice's state where its index says")
{
	WaveTables tables;
	const AudioDevice::WaveBankHandle bank = tables.add_bank({ "ping", "drone" });
	const int first = tables.add_voice(VoiceRecord{ bank, 0,
		SoundState::stopped, false });
	const int second = tables.add_voice(VoiceRecord{ bank, 1,
		SoundState::stopped, false });
	REQUIRE(first == 0);
	REQUIRE(second == 1);

	tables.voice(AudioDevice::VoiceHandle(second)).state = SoundState::playing;
	tables.voice(AudioDevice::VoiceHandle(second)).looping = true;
	CHECK(tables.voice(AudioDevice::VoiceHandle(first)).state ==
		SoundState::stopped);
	CHECK(tables.voice(AudioDevice::VoiceHandle(second)).wave == 1);
	CHECK(tables.voice(AudioDevice::VoiceHandle(second)).looping);
}

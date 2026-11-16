#include <doctest/doctest.h>

#include "engine/assets/sound_bank_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/audio/null/recording.h"
#include "engine/audio/sound_bank.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace labrador;

namespace
{
	WaveBankData memory_bank()
	{
		WaveData shot;
		shot.name = "shot";
		shot.channels = 1;
		shot.sample_rate = 22050;
		shot.samples = { -32768, -8192, 16384, 32767 };
		shot.loop_start = 1;
		shot.loop_length = 2;
		WaveData stereo;
		stereo.name = "stereo";
		stereo.channels = 2;
		stereo.sample_rate = 48000;
		stereo.samples = { -32768, 32767, -8192, 8192, 0, 16384 };
		return WaveBankData{ "generated", { std::move(shot), std::move(stereo) } };
	}
}

TEST_CASE("null memory banks own PCM after copied and moved caller storage is released")
{
	AudioDevice device;
	WaveBankData caller = memory_bank();
	const AudioDevice::WaveBankHandle copied = device.open_memory_bank(caller);
	caller.waves[0].samples.assign(32, 0);
	caller.waves.clear();
	const WaveBankData* const retained = recorded_memory_bank(device, copied);
	REQUIRE(retained != nullptr);
	REQUIRE(retained->waves.size() == 2);
	CHECK(retained->name == "generated");
	CHECK(retained->waves[0].samples == std::vector<int16_t>{ -32768, -8192, 16384, 32767 });
	CHECK(retained->waves[0].loop_start == 1);
	CHECK(retained->waves[0].loop_length == 2);
	CHECK(retained->waves[1].channels == 2);
	CHECK(retained->waves[1].samples == std::vector<int16_t>{ -32768, 32767, -8192, 8192, 0, 16384 });
	CHECK(device.wave_index(copied, "shot") == 0);
	CHECK(device.wave_index(copied, "stereo") == 1);

	const AudioDevice::WaveBankHandle legacy = device.open_wave_bank("", "legacy", { "named" });
	CHECK(recorded_memory_bank(device, legacy) == nullptr);
	WaveBankData moved = memory_bank();
	const AudioDevice::WaveBankHandle transferred = device.open_memory_bank(std::move(moved));
	moved = {};
	CHECK(recorded_memory_bank(device, copied) == retained);
	REQUIRE(recorded_memory_bank(device, transferred) != nullptr);
	CHECK(recorded_memory_bank(device, transferred)->waves[0].samples == retained->waves[0].samples);
	CHECK_THROWS_AS(recorded_memory_bank(device, AudioDevice::WaveBankHandle{}), std::out_of_range);
}

TEST_CASE("refused memory input publishes no bank and leaves existing samples and voices intact")
{
	AudioDevice device;
	const AudioDevice::WaveBankHandle first = device.open_memory_bank(memory_bank());
	const AudioDevice::VoiceHandle voice = device.create_voice(first, 0);
	device.play_voice(voice, true, 0.4f, 0.0f, 0.0f);
	WaveBankData invalid = memory_bank();
	invalid.waves[1].samples.pop_back();
	CHECK_THROWS_AS(device.open_memory_bank(std::move(invalid)), std::invalid_argument);
	CHECK(device.voice_state(voice) == SoundState::playing);
	const AudioDevice::WaveBankHandle next = device.open_memory_bank(memory_bank());
	CHECK(next.index() == first.index() + 1);
	CHECK(recorded_memory_bank(device, first)->waves[0].samples[0] == -32768);
	CHECK(recorded_sounds(device).size() == 1);
}

TEST_CASE("memory-backed SoundBank keeps levels order suspension and persistent lifecycle")
{
	AudioDevice device;
	AudioResources resources;
	const AudioDevice::WaveBankHandle handle = device.open_memory_bank(memory_bank());
	SoundBankDefinition definition;
	definition.source_path = "generated PCM";
	definition.waves = { "shot", "stereo" };
	definition.effects = { SoundBankDefinition::Effect{ "loop", "shot" } };
	resources.add_sound_bank("memory",
		build_sound_bank(&device, handle, definition));
	SoundBank* const bank = resources.sound_bank("memory");
	const SoundGroupHandle group = resources.add_sound_group("effects");
	resources.set_group_volume(group, 0.25f);
	const SoundBank::WaveHandle shot = bank->resolve_wave("shot");
	const SoundBank::EffectHandle loop = bank->resolve_effect("loop");
	bank->set_wave_group(shot, group);
	bank->set_effect_group(loop, group);
	const std::size_t begin = recorded_sounds(device).size();
	bank->play_wave(shot, 0.8f, 0.5f, -0.25f);
	bank->play_effect(loop, true, 0.6f, -0.5f, 0.75f);
	CHECK(bank->effect_state(loop) == SoundState::playing);
	bank->pause_effect(loop);
	CHECK(bank->effect_state(loop) == SoundState::paused);
	bank->resume_effect(loop);
	CHECK(bank->effect_state(loop) == SoundState::playing);
	bank->stop_effect(loop, false);
	CHECK_FALSE(bank->is_effect_looping(loop));
	CHECK(bank->effect_state(loop) == SoundState::playing);
	bank->stop_effect(loop, true);
	CHECK(bank->effect_state(loop) == SoundState::stopped);
	const std::vector<RecordedSound>& calls = recorded_sounds(device);
	REQUIRE(calls.size() == begin + 6);
	CHECK(calls[begin].call == SoundCall::play_wave);
	CHECK(calls[begin].volume == doctest::Approx(0.2f));
	CHECK(calls[begin].pitch == 0.5f);
	CHECK(calls[begin].pan == -0.25f);
	CHECK(calls[begin + 1].call == SoundCall::play_voice);
	CHECK(calls[begin + 1].volume == doctest::Approx(0.15f));
	CHECK(calls[begin + 1].loop);
	CHECK(calls[begin + 2].call == SoundCall::pause_voice);
	CHECK(calls[begin + 3].call == SoundCall::resume_voice);
	CHECK(calls[begin + 4].call == SoundCall::stop_voice);
	CHECK_FALSE(calls[begin + 4].immediate);
	CHECK(calls[begin + 5].immediate);
	const std::size_t before_suspend = calls.size();
	device.suspend();
	bank->play_wave(shot);
	CHECK(recorded_sounds(device).size() == before_suspend);
	device.resume();
	bank->play_wave(shot);
	CHECK(recorded_sounds(device).size() == before_suspend + 1);
}

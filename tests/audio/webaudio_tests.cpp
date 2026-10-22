#include <doctest/doctest.h>

#include "engine/assets/sound_bank_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/audio/sound_bank.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace labrador;

// The Web Audio backend where there is no Web Audio, which is Node.
//
// COMPILED WHEN THE BACKEND IS webaudio, and that is a browser build, whose
// tests run under Node. There the device has no AudioContext and never will,
// so these cases are everything engine/audio/webaudio/ answers without a
// sound: the container read through the engine's reader, the two throws the
// seam tells apart by type, and DirectXTK's state transitions, which this
// backend keeps on the C++ side so that what a game is told does not wait on
// the browser. What reaches a speaker needs a page and a person, and the
// commit that added this file says how it was checked.
//
// The bank is the audio sample's tones.xwb, which this folder's CMakeLists
// copies beside the executable for xwb_file_tests.cpp.

namespace
{
	const std::vector<std::string> tone_names = { "ping", "chime", "drone" };

	struct Fixture
	{
		AudioDevice device;
		AudioDevice::WaveBankHandle wave_bank;
		std::unique_ptr<SoundBank> bank;

		Fixture()
		{
			SoundBankDefinition definition;
			definition.source_path = "tones.json";
			definition.waves = tone_names;
			definition.effects = {
				SoundBankDefinition::Effect{ "drone_loop", "drone" },
				SoundBankDefinition::Effect{ "ping_once", "ping" }
			};

			this->wave_bank = this->device.open_wave_bank("./content/",
				"tones", definition.waves);
			this->bank = build_sound_bank(&this->device, this->wave_bank,
				definition);
		}
	};
}

TEST_CASE("a bank is read from its container, and its order is the file's")
{
	AudioDevice device;

	// Listed out of order on purpose: the index is where the wave sits in
	// the container, not where the definition happened to name it.
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "tones", { "drone", "ping" });

	CHECK(device.wave_index(bank, "ping") == 0);
	CHECK(device.wave_index(bank, "chime") == 1);
	CHECK(device.wave_index(bank, "drone") == 2);
	CHECK(device.wave_index(bank, "thud") == -1);
}

TEST_CASE("a missing container and a missing wave are different throws")
{
	AudioDevice device;

	// runtime_error, which is what ResourceLoader catches to substitute a
	// silent bank for an optional one - even though the engine's reader says
	// "no file" with an out_of_range, which this backend must not pass on.
	CHECK_THROWS_AS(device.open_wave_bank("./content/", "absent", tone_names),
		std::runtime_error);
	try
	{
		device.open_wave_bank("./content/", "absent", tone_names);
	}
	catch (const std::out_of_range&)
	{
		FAIL("a missing container read as a content bug");
	}
	catch (const std::runtime_error& error)
	{
		CHECK(std::string(error.what()).find("absent.xwb") !=
			std::string::npos);
	}

	// out_of_range, the content bug: the container is there and does not
	// hold a wave the definition names.
	CHECK_THROWS_AS(device.open_wave_bank("./content/", "tones",
		{ "ping", "thud" }), std::out_of_range);
}

TEST_CASE("a wave or a voice the bank does not hold is refused")
{
	AudioDevice device;
	const AudioDevice::WaveBankHandle bank = device.open_wave_bank(
		"./content/", "tones", tone_names);

	CHECK_THROWS_AS(device.create_voice(bank, 3), std::out_of_range);
	CHECK_THROWS_AS(device.create_voice(bank, -1), std::out_of_range);
	CHECK_THROWS_AS(device.play_wave(bank, 3, 1.0f, 0.0f, 0.0f),
		std::out_of_range);
	CHECK_THROWS_AS(device.voice_state(AudioDevice::VoiceHandle()),
		std::out_of_range);
	CHECK_THROWS_AS(device.wave_index(AudioDevice::WaveBankHandle(), "ping"),
		std::out_of_range);

	// And one that it does hold plays without a sound to play through.
	CHECK_NOTHROW(device.play_wave(bank, 0, 1.0f, 0.5f, -0.5f));
}

TEST_CASE("a voice moves through DirectXTK's states with no context behind it")
{
	const Fixture fixture;
	const SoundBank::EffectHandle drone =
		fixture.bank->resolve_effect("drone_loop");

	CHECK(fixture.bank->effect_state(drone) == SoundState::stopped);

	fixture.bank->play_effect(drone, true, 0.5f, 0.25f, -0.5f);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK(fixture.bank->is_effect_looping(drone));

	fixture.bank->pause_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::paused);

	// Pausing a paused voice changes nothing, and neither does resuming a
	// playing one.
	fixture.bank->pause_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::paused);

	// Play on a paused voice resumes it and keeps its loop, whatever loop
	// it is asked for, which is DirectXTK's Play.
	fixture.bank->play_effect(drone, false);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK(fixture.bank->is_effect_looping(drone));

	fixture.bank->resume_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);

	fixture.bank->set_effect_volume(drone, 0.75f);
	fixture.bank->set_effect_pitch(drone, -1.0f);
	fixture.bank->set_effect_pan(drone, 1.0f);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);

	// Not immediate on a looping voice: the loop ends and the pass plays on.
	fixture.bank->stop_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK_FALSE(fixture.bank->is_effect_looping(drone));

	fixture.bank->stop_effect(drone, true);
	CHECK(fixture.bank->effect_state(drone) == SoundState::stopped);

	// A stopped voice resumes nothing, and plays again from the start with
	// the loop it is now asked for.
	fixture.bank->resume_effect(drone);
	CHECK(fixture.bank->effect_state(drone) == SoundState::stopped);
	fixture.bank->play_effect(drone, false);
	CHECK(fixture.bank->effect_state(drone) == SoundState::playing);
	CHECK_FALSE(fixture.bank->is_effect_looping(drone));
}

TEST_CASE("a playing voice keeps the loop it was started with")
{
	const Fixture fixture;
	const SoundBank::EffectHandle ping =
		fixture.bank->resolve_effect("ping_once");

	fixture.bank->play_effect(ping, false);
	fixture.bank->play_effect(ping, true);
	CHECK(fixture.bank->effect_state(ping) == SoundState::playing);
	CHECK_FALSE(fixture.bank->is_effect_looping(ping));

	// With no clock, a voice that is not looping never reaches its end, so
	// it answers playing until it is stopped - the same answer the null
	// backend gives, for the same reason.
	fixture.bank->stop_effect(ping);
	CHECK(fixture.bank->effect_state(ping) == SoundState::playing);
}

TEST_CASE("a group's volume reaches this device as plain level calls")
{
	// The group arithmetic is SoundBank's, above the seam, so all this
	// backend sees is the product through the verbs it already has - and
	// a paused voice taking a new level is one of them.
	AudioDevice device;
	SoundBankDefinition definition;
	definition.source_path = "tones.json";
	definition.waves = tone_names;
	definition.effects = {
		SoundBankDefinition::Effect{ "drone_loop", "drone" }
	};
	const AudioDevice::WaveBankHandle wave_bank = device.open_wave_bank(
		"./content/", "tones", definition.waves);

	AudioResources resources;
	resources.add_sound_bank("tones",
		build_sound_bank(&device, wave_bank, definition));
	SoundBank* bank = resources.sound_bank("tones");
	const AudioResources::SoundGroupHandle music =
		resources.add_sound_group("music");

	const SoundBank::WaveHandle ping = bank->resolve_wave("ping");
	const SoundBank::EffectHandle drone = bank->resolve_effect("drone_loop");
	CHECK_NOTHROW(bank->set_wave_group(ping, music));
	CHECK_NOTHROW(bank->set_effect_group(drone, music));

	CHECK_NOTHROW(resources.set_group_volume(music, 0.5f));
	CHECK_NOTHROW(bank->play_wave(ping, 0.5f));
	CHECK_NOTHROW(bank->play_effect(drone, true, 0.5f));
	CHECK_NOTHROW(resources.set_group_volume(music, 0.25f));
	bank->pause_effect(drone);
	CHECK_NOTHROW(resources.set_group_volume(music, 1.0f));
	CHECK(bank->effect_state(drone) == SoundState::paused);
	bank->resume_effect(drone);
	CHECK(bank->effect_state(drone) == SoundState::playing);
}

TEST_CASE("devices are independent, and survive a move")
{
	AudioDevice first;
	AudioDevice second;
	const AudioDevice::WaveBankHandle bank = first.open_wave_bank(
		"./content/", "tones", tone_names);
	const AudioDevice::VoiceHandle voice = first.create_voice(bank, 2);
	first.play_voice(voice, true, 1.0f, 0.0f, 0.0f);

	// The second device has no banks of its own: the first one's handle
	// indexes nothing there.
	CHECK_THROWS_AS(second.wave_index(bank, "drone"), std::out_of_range);

	AudioDevice moved(std::move(first));
	CHECK(moved.voice_state(voice) == SoundState::playing);
	CHECK(moved.is_voice_looping(voice));

	moved.suspend();
	moved.resume();
	moved.update();
	CHECK(moved.voice_state(voice) == SoundState::playing);
}

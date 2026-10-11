#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/assets/asset_manifest_loader.h"
#include "engine/assets/sound_bank_loader.h"
#include "samples/audio/sound_board.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(LABRADOR_AUDIO_SAMPLE_NULL)
#include "engine/audio/null/recording.h"
#endif

#if defined(LABRADOR_AUDIO_SAMPLE_WEBAUDIO)
#include "engine/app/content_root.h"
#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_resources.h"

#include <utility>
#endif

using namespace labrador;
using namespace audio_sample;

namespace
{
	SoundBankDefinition sample_definition()
	{
		return read_sound_bank_definition(
			LABRADOR_AUDIO_SAMPLE_CONTENT "/sounds/tones.json");
	}
}

TEST_CASE("the audio example declares required content and the named loop")
{
	const AssetManifest manifest = read_asset_manifest(
		LABRADOR_AUDIO_SAMPLE_CONTENT "/manifest.json");
	bool found = false;
	for (const AssetEntry& entry : manifest.entries)
	{
		if (entry.kind == "sound_bank")
		{
			CHECK(entry.name == "tones");
			CHECK(entry.directory == "./sounds/");
			CHECK_FALSE(entry.optional);
			found = true;
		}
	}
	CHECK(found);
	const SoundBankDefinition definition = sample_definition();
	const std::vector<std::string> waves{ "ping", "chime", "drone" };
	CHECK(definition.waves == waves);
	REQUIRE(definition.effects.size() == 1);
	CHECK(definition.effects[0].name == "drone_loop");
	CHECK(definition.effects[0].wave == "drone");
}

TEST_CASE("the audio example refuses the missing-content substitute")
{
	std::unique_ptr<SoundBank> silent = SoundBank::silent();
	CHECK_THROWS_AS(SoundBoard(silent.get()), std::runtime_error);
	CHECK_THROWS_AS(SoundBoard(nullptr), std::runtime_error);
}

#if defined(LABRADOR_AUDIO_SAMPLE_NULL)
TEST_CASE("the sample routes one-shots and keeps its loop pause separate from stop")
{
	AudioDevice device;
	const SoundBankDefinition definition = sample_definition();
	const AudioDevice::WaveBankHandle handle = device.open_wave_bank(
		"unused/", "tones", definition.waves);
	std::unique_ptr<SoundBank> bank = build_sound_bank(&device, handle, definition);
	SoundBoard board(bank.get());
	board.toggle_pause();
	CHECK(board.loop_state() == SoundState::stopped);
	CHECK(recorded_sounds(device).empty());
	board.play_ping();
	board.play_chime();
	board.start_loop();
	const std::vector<RecordedSound>& sounds = recorded_sounds(device);
	REQUIRE(sounds.size() == 3);
	CHECK(sounds[0].call == SoundCall::play_wave);
	CHECK(sounds[0].bank == handle);
	CHECK(sounds[0].wave == 0);
	CHECK(sounds[1].wave == 1);
	CHECK(sounds[2].call == SoundCall::play_voice);
	CHECK(sounds[2].loop);
	CHECK(board.loop_state() == SoundState::playing);
	board.toggle_pause();
	CHECK(board.loop_state() == SoundState::paused);
	board.toggle_pause();
	CHECK(board.loop_state() == SoundState::playing);
	board.stop_loop();
	CHECK(board.loop_state() == SoundState::stopped);
	CHECK(recorded_sounds(device).back().immediate);
}

TEST_CASE("sample levels affect the persistent voice and the next one-shot")
{
	AudioDevice device;
	const SoundBankDefinition definition = sample_definition();
	const AudioDevice::WaveBankHandle handle = device.open_wave_bank(
		"unused/", "tones", definition.waves);
	std::unique_ptr<SoundBank> bank = build_sound_bank(&device, handle, definition);
	{
		SoundBoard board(bank.get());
		board.start_loop();
		board.set_volume(2.0f);
		board.set_pan(-2.0f);
		board.set_pitch(0.5f);
		board.play_chime();
		CHECK(board.volume() == 1.0f);
		CHECK(board.pan() == -1.0f);
		const std::vector<RecordedSound>& sounds = recorded_sounds(device);
		REQUIRE(sounds.size() == 5);
		CHECK(sounds[1].call == SoundCall::set_voice_volume);
		CHECK(sounds[1].volume == 1.0f);
		CHECK(sounds[2].call == SoundCall::set_voice_pan);
		CHECK(sounds[2].pan == -1.0f);
		CHECK(sounds[3].call == SoundCall::set_voice_pitch);
		CHECK(sounds[3].pitch == 0.5f);
		CHECK(sounds[4].call == SoundCall::play_wave);
		CHECK(sounds[4].wave == 1);
		CHECK(sounds[4].volume == 1.0f);
		CHECK(sounds[4].pan == -1.0f);
		CHECK(sounds[4].pitch == 0.5f);
	}
	CHECK(recorded_sounds(device).back().call == SoundCall::stop_voice);
	CHECK(recorded_sounds(device).back().immediate);
}
#endif

#if defined(LABRADOR_AUDIO_SAMPLE_WEBAUDIO)
// The browser build's stand-in for --smoke-test, which a page cannot be given.
// The same steps smoke_test.cpp takes on Windows: the packaged manifest's
// required bank, loaded through the engine's loader and the Web Audio backend,
// both one-shots, and the drone through play, pause, resume and stop. Under
// Node the backend has no AudioContext, so this proves the bank is read and the
// sample is told what it would be told in a page, and nothing about a speaker.
TEST_CASE("the sample's bank loads through Web Audio and its drone keeps its states")
{
	AudioDevice device;
	AudioResources resources;
	ResourceLoader loader(nullptr, nullptr, &resources, &device);
	const AssetManifest manifest = anchored_to_source(read_asset_manifest(
		LABRADOR_AUDIO_SAMPLE_CONTENT "/manifest.json"));
	AssetManifest audio_manifest;
	audio_manifest.source_path = manifest.source_path;
	for (const AssetEntry& entry : manifest.entries)
	{
		if (entry.kind == "sound_bank") { audio_manifest.entries.push_back(entry); }
	}
	REQUIRE(audio_manifest.entries.size() == 1);
	loader.load_manifest(std::move(audio_manifest));

	SoundBank* bank = resources.sound_bank("tones");
	REQUIRE(bank != nullptr);
	CHECK(bank->audible());
	SoundBoard board(bank);
	CHECK_NOTHROW(board.play_ping());
	CHECK_NOTHROW(board.play_chime());
	board.start_loop();
	CHECK(board.loop_state() == SoundState::playing);
	CHECK(board.is_looping());
	board.set_volume(0.25f);
	board.set_pan(-0.5f);
	board.set_pitch(0.25f);
	device.update();
	board.toggle_pause();
	CHECK(board.loop_state() == SoundState::paused);
	board.toggle_pause();
	CHECK(board.loop_state() == SoundState::playing);
	board.stop_loop();
	CHECK(board.loop_state() == SoundState::stopped);
}
#endif

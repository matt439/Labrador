#include <doctest/doctest.h>

#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/audio/sound_bank.h"
#include "tests/diagnostic_capture.h"
#include "tests/scratch_file.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace labrador;
using labrador_tests::ScratchFile;

namespace
{
	std::vector<unsigned char> pcm_wave()
	{
		std::vector<unsigned char> bytes(52, 0);
		const char riff[] = "RIFF";
		const char wave[] = "WAVEfmt ";
		const char data[] = "data";
		for (size_t index = 0; index < 4; ++index)
		{
			bytes[index] = static_cast<unsigned char>(riff[index]);
			bytes[36 + index] = static_cast<unsigned char>(data[index]);
		}
		for (size_t index = 0; index < 8; ++index)
		{
			bytes[8 + index] = static_cast<unsigned char>(wave[index]);
		}
		labrador_tests::put_u32_le(bytes, 4, 44);
		labrador_tests::put_u32_le(bytes, 16, 16);
		bytes[20] = 1;
		bytes[22] = 1;
		labrador_tests::put_u32_le(bytes, 24, 22050);
		labrador_tests::put_u32_le(bytes, 28, 44100);
		bytes[32] = 2;
		bytes[34] = 16;
		labrador_tests::put_u32_le(bytes, 40, 8);
		bytes[46] = 0xFF;
		bytes[47] = 0x7F;
		bytes[49] = 0x80;
		bytes[51] = 0x40;
		return bytes;
	}

	AssetManifest wave_manifest(const std::string& path, bool optional = false)
	{
		const std::filesystem::path file(path);
		AssetEntry entry;
		entry.kind = "wave";
		entry.name = file.stem().string();
		entry.directory = file.parent_path().generic_string() + "/";
		entry.optional = optional;
		AssetManifest manifest;
		manifest.source_path = "wave_asset_tests.json";
		manifest.entries.push_back(entry);
		return manifest;
	}
}

TEST_CASE("a WAV manifest asset publishes a same-name wave and retained effect without a definition")
{
	const ScratchFile file("one_wave.wav", pcm_wave());
	const AssetManifest manifest = wave_manifest(file.path());
	const std::string name = manifest.entries[0].name;
	AudioDevice device;
	AudioResources audio;
	ResourceLoader loader(nullptr, nullptr, &audio, &device);
	loader.load_manifest(manifest);
	SoundBank* bank = audio.sound_bank(name);
	REQUIRE(bank->audible());
	const SoundBank::WaveHandle wave = bank->resolve_wave(name);
	const SoundBank::EffectHandle effect = bank->resolve_effect(name);
	CHECK(std::filesystem::remove(file.path()));
	CHECK_NOTHROW(bank->play_wave(wave, 0.25f, 0.5f, -0.25f));
	bank->play_effect(effect, true);
	CHECK(bank->effect_state(effect) == SoundState::playing);
	bank->pause_effect(effect);
	CHECK(bank->effect_state(effect) == SoundState::paused);
	bank->resume_effect(effect);
	CHECK(bank->effect_state(effect) == SoundState::playing);
	bank->stop_effect(effect, true);
	CHECK(bank->effect_state(effect) == SoundState::stopped);
	// Audio is retained through graphics restoration, without reading the now-missing file.
	CHECK_NOTHROW(loader.reload_device_resources());
	CHECK(audio.sound_bank(name) == bank);
	CHECK_THROWS_AS(loader.load_manifest(manifest), std::out_of_range);
	CHECK(audio.sound_bank(name) == bank);
}

TEST_CASE("required missing or malformed WAV assets fail before publication and name the file")
{
	std::vector<unsigned char> unsupported = pcm_wave();
	unsupported[20] = 3;
	for (const std::vector<unsigned char>& bytes : { std::vector<unsigned char>{}, unsupported })
	{
		const ScratchFile file("bad_wave.wav", bytes);
		const AssetManifest manifest = wave_manifest(file.path());
		AudioDevice device;
		AudioResources audio;
		ResourceLoader loader(nullptr, nullptr, &audio, &device);
		try
		{
			loader.load_manifest(manifest);
			FAIL("malformed required WAV was published");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find(std::filesystem::path(file.path()).generic_string()) != std::string::npos);
		}
		CHECK_THROWS_AS(audio.sound_bank(manifest.entries[0].name), std::out_of_range);
	}
	const ScratchFile missing("missing_wave.wav", pcm_wave());
	CHECK(std::filesystem::remove(missing.path()));
	AudioDevice device;
	AudioResources audio;
	ResourceLoader loader(nullptr, nullptr, &audio, &device);
	const AssetManifest manifest = wave_manifest(missing.path());
	CHECK_THROWS_AS(loader.load_manifest(manifest), std::out_of_range);
	CHECK_THROWS_AS(audio.sound_bank(manifest.entries[0].name), std::out_of_range);
}

TEST_CASE("unavailable optional WAV containers report their reason and substitute a silent bank")
{
	for (const bool missing : { false, true })
	{
		const ScratchFile file("optional_wave.wav", std::vector<unsigned char>{});
		if (missing) CHECK(std::filesystem::remove(file.path()));
		labrador_tests::DiagnosticCapture capture;
		AudioDevice device;
		AudioResources audio;
		ResourceLoader loader(nullptr, nullptr, &audio, &device);
		const AssetManifest manifest = wave_manifest(file.path(), true);
		loader.load_manifest(manifest);
		SoundBank* bank = audio.sound_bank(manifest.entries[0].name);
		CHECK_FALSE(bank->audible());
		CHECK_NOTHROW(bank->play_wave(bank->resolve_wave(manifest.entries[0].name)));
		CHECK(capture.calls() == 1);
		CHECK(capture.messages().find(std::filesystem::path(file.path()).generic_string()) != std::string_view::npos);
		CHECK(capture.messages().find("marks this wave optional") != std::string_view::npos);
	}
}

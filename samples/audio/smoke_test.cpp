#include "samples/audio/smoke_test.h"

#include "engine/app/content_root.h"
#include "engine/assets/asset_manifest_loader.h"
#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/audio/wav_file.h"
#include "samples/audio/sound_board.h"

#include <Windows.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

using namespace labrador;

namespace audio
{
	namespace
	{
		void report_step(const char* step)
		{
			std::printf("AudioSample smoke: %s\n", step);
			std::fflush(stdout);
		}

		void require(bool condition, const char* message)
		{
			if (!condition) { throw std::runtime_error(message); }
		}

		void update_for(AudioDevice& device, int milliseconds)
		{
			for (int elapsed = 0; elapsed < milliseconds; elapsed += 10)
			{
				device.update();
				Sleep(10);
			}
		}

		void verify_owned_region(AudioDevice& device, const std::string& path)
		{
			AudioDevice::WaveBankHandle bank;
			{
				WaveData wave = read_wav_file(path);
				wave.name = "region";
				wave.loop_start = 32;
				wave.loop_length = 128;
				WaveBankData data;
				data.name = "owned_region";
				data.waves.push_back(std::move(wave));
				bank = device.open_memory_bank(std::move(data));
			}
			// Caller containers are gone before any voice consumes their PCM.
			const AudioDevice::VoiceHandle voice = device.create_voice(bank, 0);
			device.play_voice(voice, true, 0.02f, 0.0f, 0.0f);
			require(device.voice_state(voice) == SoundState::playing,
				"The owned PCM region did not start: an active audio output is required");
			update_for(device, 50);
			device.pause_voice(voice);
			require(device.voice_state(voice) == SoundState::paused, "The owned PCM region did not pause");
			device.resume_voice(voice);
			require(device.voice_state(voice) == SoundState::playing, "The owned PCM region did not resume");
			device.stop_voice(voice, true);
			require(device.voice_state(voice) == SoundState::stopped, "The owned PCM region did not stop");
		}
	}

	void smoke_test()
	{
		report_step("opening audio device");
		AudioDevice device;
		AudioResources resources;
		ResourceLoader loader(nullptr, nullptr, &resources, &device);
		const std::string manifest_path = executable_directory() + "manifest.json";
		const AssetManifest manifest = anchored_to_source(
			read_asset_manifest(manifest_path.c_str()));
		AssetManifest audio_manifest = audio_entries(manifest);
		for (const AssetEntry& entry : audio_manifest.entries)
		{
			require(!entry.optional, "The audio assets must be required");
		}
		report_step("loading required bank");
		loader.load_manifest(std::move(audio_manifest));
		report_step("checking transferred PCM loop region");
		verify_owned_region(device, executable_directory() + "waves/ping_wav.wav");
		SoundBoard board(resources.sound_bank("tones"));
		SoundBank* wav = resources.sound_bank("ping_wav");
		require(wav->audible(), "The sample requires the ping_wav WAV asset");
		report_step("playing WAV ping");
		wav->play_wave(wav->resolve_wave("ping_wav"), board.volume(),
			board.pitch(), board.pan());
		update_for(device, 200);
		const SoundBank::EffectHandle wav_effect = wav->resolve_effect("ping_wav");
		report_step("starting WAV effect");
		wav->play_effect(wav_effect, true, board.volume());
		require(wav->effect_state(wav_effect) == SoundState::playing,
			"The WAV effect did not start: an active audio output device is required");
		update_for(device, 50);
		wav->pause_effect(wav_effect);
		require(wav->effect_state(wav_effect) == SoundState::paused, "The WAV effect did not pause");
		wav->resume_effect(wav_effect);
		require(wav->effect_state(wav_effect) == SoundState::playing, "The WAV effect did not resume");
		wav->stop_effect(wav_effect, true);
		require(wav->effect_state(wav_effect) == SoundState::stopped, "The WAV effect did not stop");
		report_step("playing ping");
		board.play_ping();
		update_for(device, 200);
		report_step("playing chime");
		board.play_chime();
		update_for(device, 450);
		report_step("starting loop");
		board.start_loop();
		require(board.loop_state() == SoundState::playing && board.is_looping(),
			"The drone did not start: an active audio output device is required");
		update_for(device, 200);
		board.set_volume(0.25f);
		board.set_pan(-0.5f);
		board.set_pitch(0.25f);
		report_step("pausing loop");
		board.toggle_pause();
		require(board.loop_state() == SoundState::paused, "The drone did not pause");
		update_for(device, 50);
		report_step("resuming loop");
		board.toggle_pause();
		require(board.loop_state() == SoundState::playing, "The drone did not resume");
		update_for(device, 200);
		report_step("stopping loop");
		board.stop_loop();
		require(board.loop_state() == SoundState::stopped, "The drone did not stop");
#if defined(LABRADOR_AUDIO_SAMPLE_NULL)
		std::puts("AudioSample smoke passed (null audio: recorded calls, no playback).");
#else
		std::puts("AudioSample smoke passed (XAudio2 XWB/WAV banks and playback lifecycle).");
#endif
	}
}

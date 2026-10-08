#include "samples/audio/smoke_test.h"

#include "engine/app/content_root.h"
#include "engine/assets/asset_manifest_loader.h"
#include "engine/assets/resource_loader.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "samples/audio/sound_board.h"

#include <Windows.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

using namespace labrador;

namespace audio_sample
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
		AssetManifest audio_manifest;
		audio_manifest.source_path = manifest.source_path;
		for (const AssetEntry& entry : manifest.entries)
		{
			if (entry.kind == "sound_bank")
			{
				require(!entry.optional, "AudioSample's bank must be required.");
				audio_manifest.entries.push_back(entry);
			}
		}
		report_step("loading required bank");
		loader.load_manifest(std::move(audio_manifest));
		SoundBoard board(resources.sound_bank("tones"));
		report_step("playing ping");
		board.play_ping();
		update_for(device, 200);
		report_step("playing chime");
		board.play_chime();
		update_for(device, 450);
		report_step("starting loop");
		board.start_loop();
		require(board.loop_state() == SoundState::playing && board.is_looping(),
			"The drone did not start. An active audio output device is required.");
		update_for(device, 200);
		board.set_volume(0.25f);
		board.set_pan(-0.5f);
		board.set_pitch(0.25f);
		report_step("pausing loop");
		board.toggle_pause();
		require(board.loop_state() == SoundState::paused, "The drone did not pause.");
		update_for(device, 50);
		report_step("resuming loop");
		board.toggle_pause();
		require(board.loop_state() == SoundState::playing, "The drone did not resume.");
		update_for(device, 200);
		report_step("stopping loop");
		board.stop_loop();
		require(board.loop_state() == SoundState::stopped, "The drone did not stop.");
#if defined(LABRADOR_AUDIO_SAMPLE_NULL)
		std::puts("AudioSample smoke passed (null audio: recorded calls, no playback).");
#else
		std::puts("AudioSample smoke passed (XAudio2 bank and playback lifecycle).");
#endif
	}
}

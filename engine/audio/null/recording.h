#pragma once

#include "engine/audio/audio_device.h"

#include <string>
#include <vector>

// The null backend's audio calls, for headless tests.
//
// Tests built with null audio may include this header.
// cmake/check_engine_includes.cmake scans engine/ and forbids includes from
// outside engine/audio/null/; this recording interface grants no exception to
// engine code. Its fields name no platform audio API.
//
// Records expose call order, bank and wave identity, voice handles and levels
// passed through the seam. SoundBank's level clamping can be checked here;
// SoundBank::silent() declines playback instead of recording it.
//
// There is no mixer or clock, so this backend cannot establish audible output,
// overlap, relative loudness or natural completion. voice_state() follows calls:
// a non-looping voice does not finish by itself, and stop_voice(false) does not
// stop its tail. A looping voice's non-immediate stop clears its loop flag;
// an immediate stop changes the state to stopped. This models explicit state
// transitions without the asynchronous buffer completion XAudio2 reports.
//
// open_wave_bank stores the supplied wave names without opening a container.
// Missing-container failures and ResourceLoader's optional-bank fallback to
// SoundBank::silent() therefore cannot be exercised through this backend.

namespace labrador
{
	// Which of the seam's verbs was called. One enumerator per method that
	// changes something; wave_index, voice_state and is_voice_looping are
	// questions and are not recorded, because their answers are the assertion.
	enum class SoundCall
	{
		play_wave,
		play_voice,
		stop_voice,
		pause_voice,
		resume_voice,
		set_voice_volume,
		set_voice_pitch,
		set_voice_pan
	};

	// One call, exactly as the seam received it.
	//
	// Every call fills bank and wave; voice calls also fill voice. play_wave
	// and play_voice fill the three levels, and play_voice also fills loop.
	// Level setters fill the level they set, and stop_voice fills immediate.
	// Fields unused by the call retain the defaults below.
	struct RecordedSound
	{
		SoundCall call = SoundCall::play_wave;

		AudioDevice::WaveBankHandle bank;
		int wave = -1;

		AudioDevice::VoiceHandle voice;
		bool loop = false;
		bool immediate = false;

		float volume = 0.0f;
		float pitch = 0.0f;
		float pan = 0.0f;
	};

	// Everything asked of `device` since it was constructed, in call order.
	//
	// The device owns the vector; adding a call may invalidate references to
	// its entries. There is no clearing verb or frame boundary: construct a new
	// device for an empty recording.
	const std::vector<RecordedSound>& recorded_sounds(const AudioDevice& device);

	// The supplied wave names, in definition order; these indices are the
	// answers wave_index returns. The bank handle must resolve on this device.
	// The device owns the vector; opening another bank may invalidate this
	// reference. Copy names needed across that operation or device destruction.
	const std::vector<std::string>& recorded_wave_names(
		const AudioDevice& device, AudioDevice::WaveBankHandle bank);
}

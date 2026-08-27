#pragma once

#include "engine/audio/audio_device.h"

#include <string>
#include <vector>

// What the null backend was asked to play.
//
// THIS HEADER IS MEANT TO BE INCLUDED FROM OUTSIDE ITS FOLDER, exactly as
// engine/render/null/recording.h is, and for the same reason and under the same
// wall. cmake/check_engine_includes.cmake fails the build for any file outside
// engine/<module>/<backend>/ that names a header inside it, and this file is
// inside that folder rather than outside it. What keeps it legal is narrow:
// every includer is a test, the check scans engine/ only, and anything in
// engine/ that reached for this would fail the build and correctly - a
// recording is for a test to read. Nothing here names an audio API. A
// RecordedSound is two handles, an index, two flags and three floats, every one
// of them a type engine/audio/ already hands out.
//
// WHY THIS BACKEND EXISTS, AND WHY ITS ABSENCE WAS THE FINDING.
// PHILOSOPHY.md:632-637 requires a seam to ship with a headless implementation,
// "because a seam with only the platform's own implementation behind it still
// requires the platform in order to construct anything". Audio was the standing
// counter-example in this tree, and SoundBank::silent() was the thing that made
// it look answered: a null DirectX::WaveBank pointer inside DirectXTK's own
// class, checked at the top of every method, which could DECLINE to play a
// sound and could not RECORD one. Eight of SoundBank's thirteen instance
// methods had no observable behaviour anywhere in this repository as a result,
// and "it did not throw" was the whole of what a test could assert about them -
// tests/audio/sound_bank_tests.cpp carried the list and docs/next.md 3.4a is
// where it was asked for. This file is the other half of that list: with a
// device that records, those eight say what they did, and the five sites of
// level clamping that had never executed are readable in the numbers below.
//
// IT RECORDS WHAT WAS ASKED FOR, NOT WHAT IT SOUNDED LIKE, and that is a floor
// rather than a first version. There is no mixer here, no sample rate and no
// clock - so nothing in this backend can tell you that two sounds overlapped,
// that one was louder than the other in any physical sense, or that a wave
// finished. What a test can assert is that an object asked for the right wave,
// out of the right bank, with the levels the engine's own arithmetic produced,
// in the order it asked - which is the audio half of what
// engine/render/null/recording.h says about quads.
//
// THE ONE PLACE THE MODEL IS WEAKER THAN THE REAL ONE, SAID OUT LOUD.
// voice_state() is answered from the transitions below and there is no time in
// them, where XAudio2's state is partly asynchronous: DirectXTK's
// SoundEffectInstance::GetState auto-stops a voice whose buffer has drained,
// and Stop(immediate = false) on a voice that is not looping asks XAudio2 to
// play the tail and leaves the state PLAYING until it does. A voice this
// backend was asked to play therefore stays playing for ever unless something
// stops it, and a test that expects a sound to have finished on its own is
// asserting something no backend here can answer. That difference is
// transcribed from DirectXTK's SoundCommon.h rather than guessed at, and it is
// the reason this file states it instead of leaving the two to drift.
//
// WHAT IT STILL CANNOT EXERCISE. The substitution a missing container triggers:
// there is no container here, so open_wave_bank cannot fail, and
// ResourceLoader's optional-bank path - the one that reports "no audio" and
// installs SoundBank::silent() - has no way to run in this configuration. That
// is deliberate. A backend that could be asked to fail on purpose is test
// machinery on the platform side of a seam, and engine/render/null/ does not
// have any either.

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
	// Only the fields the call carries are filled in; the rest keep the
	// defaults below. play_wave fills bank, wave and the three levels;
	// play_voice fills voice, loop and the three levels; the three level
	// setters fill voice and the one level they set; stop_voice fills voice and
	// immediate; pause_voice and resume_voice fill voice alone.
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
	// There is no clearing verb and no frame boundary to hang one on: a device
	// is cheap here - it opens no hardware - so a case that wants an empty
	// recording constructs one rather than emptying one.
	const std::vector<RecordedSound>& recorded_sounds(const AudioDevice& device);

	// The wave names `device` was told a bank holds, in the order the
	// definition listed them - which on this backend IS the bank, because there
	// is no container to read one out of. Its indices are what wave_index
	// answers with.
	const std::vector<std::string>& recorded_wave_names(
		const AudioDevice& device, AudioDevice::WaveBankHandle bank);
}

#pragma once

#include "engine/core/handle.h"

#include <memory>
#include <string>
#include <vector>

namespace labrador
{
	// What a voice is doing, in this engine's words rather than a library's.
	//
	// It is three values because that is what the thing has: a voice is
	// stopped, playing, or paused, and every audio API this engine is likely
	// to sit on spells those three. It exists at all because the type it
	// replaces was DirectX::SoundState - an XAudio2 library's enum, returned
	// from a public engine method, so a game asking what its music was doing
	// had to name a platform to ask.
	enum class SoundState
	{
		stopped,
		playing,
		paused
	};

	// The audio backend seam.
	//
	// A concrete class with one implementation chosen at build time, for the
	// reasons renderer.h gives at length and gamepad_reader.h repeats in two
	// sentences: asking for a backend that was not built is a missing symbol
	// rather than a run-time answer (T5), a vtable is a tax nothing has asked
	// to pay (T8), and promoting a concrete class to an interface later is
	// mechanical and changes no call site. The XAudio2/DirectXTK implementation
	// is engine/audio/xaudio2/, the headless one is engine/audio/null/, and
	// nothing outside those folders names an audio API.
	//
	// WHY IT EXISTS, WHICH IS NOT THE SAME AS WHY THE OTHER TWO SEAMS DO.
	// ARCHITECTURE's module table has promised "audio | core, math - the audio
	// backend at its edge only" since it was written, and there was no edge:
	// DirectXTK's <Audio.h> was in the public headers of three modules, and the
	// engine's own vocabulary was spelt in its types - a SoundBank was
	// constructed from a DirectX::WaveBank, an effect handle was a
	// Registry<DirectX::SoundEffectInstance>::handle, and effect_state answered
	// with a DirectX::SoundState. docs/port/android.md 3.2 called that "the one
	// place the second-platform claim is provably false today". That is the
	// architectural half.
	//
	// AND THE MEASURED HALF. PHILOSOPHY.md:632-637 requires that "a seam ships
	// with its headless implementation, or it has not shipped", because "a seam
	// with only the platform's own implementation behind it still requires the
	// platform in order to construct anything". SoundBank::silent() does not
	// satisfy that rule and must not be mistaken for something that does: it is
	// a bank with no content, which is a question about content and not about
	// the platform. engine/audio/null/ is the headless implementation, and
	// engine/audio/null/recording.h says what it makes assertable.
	//
	// WHERE THE CUT IS, AND WHAT IS DELIBERATELY ABOVE IT. Everything this
	// engine decides is above: which name means which wave, which handles are
	// valid, and the clamp that folds a volume into [0,1] and a pitch and a pan
	// into [-1,1]. Levels arrive here already clamped and handles arrive here
	// already checked. Below is what an audio API actually does - open a
	// container, find a name in it, build a voice, and start, stop, adjust or
	// report one - and nothing else. That line is the one thing 3.4a measured
	// that a reader of sound_bank.cpp would have had to count by hand.
	//
	// WHAT THE SEAM DOES NOT DECIDE IS THE CONTAINER, AND THAT IS DELIBERATE.
	// docs/survey/2026-08-26.md 6 leaves the .xwb question open - whether the
	// wave-bank format moves with a port or the seam is cut above it with the
	// format decision deferred - and this is cut above it. open_wave_bank
	// takes a directory and a bank name, never a file name: the extension,
	// the reader and the bytes are the backend's business, exactly as a
	// swapchain is render/'s. What crosses instead is the list of wave NAMES
	// the definition beside the container says it holds, because that list is
	// content this engine parses out of its own JSON. A backend with no
	// container answers from that list; a backend with one checks the list
	// against it and throws naming the wave that is missing. Writing the
	// container format down - an xwb_file.h beside dds_file.h, which is what
	// dds_file.h's own precedent argues for - stays open, and is blocked on
	// something this repository does not have: there is no .xwb in this tree
	// to write a reader against.
	class AudioDevice
	{
	public:
		// A wave bank the device has open. The tag is never defined, like
		// SoundBank::Wave: it exists so a handle to a bank cannot be passed
		// where a handle to a voice belongs (core/handle.h).
		struct WaveBankResource;
		using WaveBankHandle = Handle<WaveBankResource>;

		// A voice: one persistent playing instance of one wave, which is what
		// sound_bank.h means by an effect instance. A one-shot has no handle,
		// because it has nothing anybody can do to it afterwards.
		struct Voice;
		using VoiceHandle = Handle<Voice>;

		AudioDevice();
		~AudioDevice();

		AudioDevice(AudioDevice&&) noexcept;
		AudioDevice& operator=(AudioDevice&&) noexcept;

		// Load-time. Opens the wave bank named `name` under `directory`, and
		// takes `wave_names` as what the definition beside it says is in there.
		//
		// Throws std::runtime_error naming the file it looked for if the
		// container is not there or cannot be read - which is the throw a
		// caller catches to substitute SoundBank::silent() for an optional
		// bank, and the reason it is a runtime_error and not a logic_error.
		//
		// Throws std::out_of_range naming the wave if the container is there
		// and does not hold a name `wave_names` lists. That is a content bug
		// rather than a missing file, it is not the same failure, and the two
		// are told apart by their type rather than by a flag.
		WaveBankHandle open_wave_bank(const std::string& directory,
			const std::string& name,
			const std::vector<std::string>& wave_names);

		// The index of `wave_name` within `bank`, or -1 if the bank has no such
		// wave. The index is the backend's own and means nothing to any other
		// bank, which is why SoundBank hands it back inside a WaveHandle.
		int wave_index(WaveBankHandle bank, const std::string& wave_name) const;

		// Load-time. A voice that plays wave `wave_index` from `bank`, for a
		// caller that wants to stop, loop or adjust it later.
		VoiceHandle create_voice(WaveBankHandle bank, int wave_index);

		// Fire-and-forget: a new voice each time, with nothing to hold
		// afterwards. Levels arrive clamped.
		void play_wave(WaveBankHandle bank, int wave_index, float volume,
			float pitch, float pan) const;

		// The persistent voice's verbs, one apiece. Levels arrive clamped and
		// handles arrive checked, so a backend implements each of these as the
		// single call its API spells it with and nothing else.
		void play_voice(VoiceHandle voice, bool loop, float volume, float pitch,
			float pan) const;
		void stop_voice(VoiceHandle voice, bool immediate) const;
		void pause_voice(VoiceHandle voice) const;
		void resume_voice(VoiceHandle voice) const;
		void set_voice_volume(VoiceHandle voice, float volume) const;
		void set_voice_pitch(VoiceHandle voice, float pitch) const;
		void set_voice_pan(VoiceHandle voice, float pan) const;
		SoundState voice_state(VoiceHandle voice) const;
		bool is_voice_looping(VoiceHandle voice) const;

		// The shell's three, and the whole of what app/ knows about audio.
		//
		// update() is called once a frame, after the states have updated, and
		// answers nothing: a device that has been lost is a case this tree has
		// never handled and this seam does not pretend to. The backend says so
		// where it ignores the answer, which is one place rather than the
		// caller's.
		void update();

		// The window lost or regained the foreground. Same contract as
		// GamepadReader's pair and for the same reason: a game in the
		// background makes no noise, and what suspension costs the platform is
		// the platform's affair.
		void suspend();
		void resume();

		// The backend's own state, for a backend header that has to reach it.
		// Declared here and defined in engine/audio/<backend>/, so naming it is
		// a deliberate include of a backend folder rather than something a game
		// file can do by accident - engine/render/renderer.h carries the same
		// pair for the same reason. The one thing in this tree that uses it is
		// engine/audio/null/recording.h, which is how a test reads back what
		// was played, and the include wall is what keeps that the only one.
		struct Impl;
		Impl* impl() const { return this->impl_.get(); }

	private:
		std::unique_ptr<Impl> impl_;
	};
}

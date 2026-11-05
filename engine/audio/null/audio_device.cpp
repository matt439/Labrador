#include "engine/audio/audio_device.h"
#include "engine/audio/null/recording.h"
#include "engine/audio/wave_tables.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

// The null audio backend: everything the seam requires, and no audio API.
//
// THERE IS NO DEVICE, WHICH IS THE ENTIRE POINT. Nothing here opens hardware,
// reads a file or calls anything that could fail for a reason outside this
// repository, so it runs on a build machine, in a container, and on a runner
// with no sound card - which is what makes it the configuration that can assert
// what an object played rather than only that playing it did not throw.
// recording.h carries the argument for why that mattered enough to build;
// this file is the mechanism.
//
// A BANK IS THE LIST OF NAMES IT WAS HANDED, and that is the whole of the
// container question on this side of the seam. audio_device.h explains why the
// list crosses at all: the definition JSON is content engine/assets/ parses,
// the container is not, and a backend with no container still has to be able to
// answer "which wave is that" so a misspelt name fails at load the way it does
// with audio present. It is the same shape as this tree's other null backend,
// where the engine decodes a .dds and render/null/ keeps the width and the
// height - the engine parses its own content and a backend keeps the minimum it
// needs to answer questions.

namespace labrador
{
	// Each voice's state follows DirectXTK's transitions (VoiceRecord),
	// because a model that disagrees with the backend it stands in for is
	// worse than no model at all. recording.h names the one transition that
	// cannot be reproduced here and why.
	struct AudioDevice::Impl
	{
		WaveTables tables;
		std::vector<RecordedSound> recording;
		bool suspended = false;

		VoiceRecord& voice(VoiceHandle handle)
		{
			return this->tables.voice(handle);
		}

		void record(const RecordedSound& call)
		{
			this->recording.push_back(call);
		}
	};

	AudioDevice::AudioDevice() : impl_(std::make_unique<Impl>())
	{

	}

	AudioDevice::~AudioDevice() = default;
	AudioDevice::AudioDevice(AudioDevice&&) noexcept = default;
	AudioDevice& AudioDevice::operator=(AudioDevice&&) noexcept = default;

	AudioDevice::WaveBankHandle AudioDevice::open_wave_bank(
		const std::string& directory, const std::string& name,
		const std::vector<std::string>& wave_names)
	{
		// The directory and the name are what a container would have been found
		// by, and there is no container. They are named rather than dropped
		// because the signature is the seam's and not this backend's - see
		// audio_device.h, where the two throws this one cannot make are also
		// documented as the caller's contract rather than as any backend's.
		std::ignore = directory;
		std::ignore = name;

		return this->impl_->tables.add_bank(wave_names);
	}

	int AudioDevice::wave_index(WaveBankHandle bank,
		const std::string& wave_name) const
	{
		// A linear walk over a list a definition wrote by hand is load-time
		// work on a handful of names, and this backend is not where anybody
		// measures anything.
		return this->impl_->tables.wave_index(bank, wave_name);
	}

	AudioDevice::VoiceHandle AudioDevice::create_voice(WaveBankHandle bank,
		int wave_index)
	{
		this->impl_->tables.check_voice_wave(bank, wave_index);
		return VoiceHandle(this->impl_->tables.add_voice(VoiceRecord{ bank,
			wave_index, SoundState::stopped, false }));
	}

	void AudioDevice::play_wave(WaveBankHandle bank, int wave_index,
		float volume, float pitch, float pan) const
	{
		// The seam's bounds check, which every backend makes with this throw
		// rather than leaving it to its API: DirectXTK's Play drops an index
		// past the end without a word. A wave index that came from nowhere, or
		// from another bank, fails here rather than being recorded as fine.
		this->impl_->tables.check_played_wave(bank, wave_index);

		// A one-shot asked for while suspended is dropped on every backend
		// (audio_device.h), so it is not recorded as played.
		if (this->impl_->suspended)
		{
			return;
		}

		RecordedSound call;
		call.call = SoundCall::play_wave;
		call.bank = bank;
		call.wave = wave_index;
		call.volume = volume;
		call.pitch = pitch;
		call.pan = pan;
		this->impl_->record(call);
	}

	void AudioDevice::play_voice(VoiceHandle voice, bool loop, float volume,
		float pitch, float pan) const
	{
		VoiceRecord& state = this->impl_->voice(voice);

		// DirectXTK's Play, which webaudio/ transcribes too: a paused voice
		// resumes and keeps its loop, a playing one is left alone, and only a
		// stopped one starts with the loop it was asked for. The call is
		// recorded whichever branch it took, with the loop it asked for.
		if (state.state == SoundState::paused)
		{
			state.state = SoundState::playing;
		}
		else if (state.state == SoundState::stopped)
		{
			state.state = SoundState::playing;
			state.looping = loop;
		}

		RecordedSound call;
		call.call = SoundCall::play_voice;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		call.loop = loop;
		call.volume = volume;
		call.pitch = pitch;
		call.pan = pan;
		this->impl_->record(call);
	}

	void AudioDevice::stop_voice(VoiceHandle voice, bool immediate) const
	{
		VoiceRecord& state = this->impl_->voice(voice);

		// THE THREE BRANCHES ARE DirectXTK'S, IN ITS ORDER. An immediate stop
		// is the only one that stops anything: asking a looping voice to stop
		// without it leaves the loop's current pass playing and clears the loop
		// flag, and asking a voice that is not looping leaves the tail playing
		// until a buffer drains - which is the transition this backend has no
		// clock for, and recording.h says so rather than pretending otherwise.
		if (immediate)
		{
			state.state = SoundState::stopped;
		}
		else if (state.looping)
		{
			state.looping = false;
		}

		RecordedSound call;
		call.call = SoundCall::stop_voice;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		call.immediate = immediate;
		this->impl_->record(call);
	}

	void AudioDevice::pause_voice(VoiceHandle voice) const
	{
		VoiceRecord& state = this->impl_->voice(voice);
		if (state.state == SoundState::playing)
		{
			state.state = SoundState::paused;
		}

		RecordedSound call;
		call.call = SoundCall::pause_voice;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		this->impl_->record(call);
	}

	void AudioDevice::resume_voice(VoiceHandle voice) const
	{
		VoiceRecord& state = this->impl_->voice(voice);
		if (state.state == SoundState::paused)
		{
			state.state = SoundState::playing;
		}

		RecordedSound call;
		call.call = SoundCall::resume_voice;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		this->impl_->record(call);
	}

	void AudioDevice::set_voice_volume(VoiceHandle voice, float volume) const
	{
		const VoiceRecord& state = this->impl_->voice(voice);

		RecordedSound call;
		call.call = SoundCall::set_voice_volume;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		call.volume = volume;
		this->impl_->record(call);
	}

	void AudioDevice::set_voice_pitch(VoiceHandle voice, float pitch) const
	{
		const VoiceRecord& state = this->impl_->voice(voice);

		RecordedSound call;
		call.call = SoundCall::set_voice_pitch;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		call.pitch = pitch;
		this->impl_->record(call);
	}

	void AudioDevice::set_voice_pan(VoiceHandle voice, float pan) const
	{
		const VoiceRecord& state = this->impl_->voice(voice);

		RecordedSound call;
		call.call = SoundCall::set_voice_pan;
		call.bank = state.bank;
		call.wave = state.wave;
		call.voice = voice;
		call.pan = pan;
		this->impl_->record(call);
	}

	SoundState AudioDevice::voice_state(VoiceHandle voice) const
	{
		return this->impl_->voice(voice).state;
	}

	bool AudioDevice::is_voice_looping(VoiceHandle voice) const
	{
		return this->impl_->voice(voice).looping;
	}

	void AudioDevice::update()
	{

	}

	void AudioDevice::suspend()
	{
		this->impl_->suspended = true;
	}

	void AudioDevice::resume()
	{
		this->impl_->suspended = false;
	}

	// --- recording.h ---------------------------------------------------------

	const std::vector<RecordedSound>& recorded_sounds(const AudioDevice& device)
	{
		return device.impl()->recording;
	}

	const std::vector<std::string>& recorded_wave_names(
		const AudioDevice& device, AudioDevice::WaveBankHandle bank)
	{
		return device.impl()->tables.bank(bank);
	}
}

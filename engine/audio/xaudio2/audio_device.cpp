#include "engine/audio/audio_device.h"

#include <Audio.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// The XAudio2 backend, through DirectXTK, and the only file in this repository
// that includes <Audio.h>.
//
// It used to be four public engine headers, which is what made
// docs/port/android.md 3.2 call audio "the one place the second-platform claim
// is provably false today". A game that asked what its music was doing named a
// Microsoft library to ask; a loader that built a bank handed one in. Nothing
// above audio_device.h names an audio API now, and cmake/check_engine_includes
// fails the build for a file outside this folder that reaches for one of ours.
//
// WHAT THIS FILE OWNS THAT THE SEAM DOES NOT. Three things, and they are the
// three the seam declines to decide:
//
//  - THE CONTAINER. A wave bank is an .xwb here, an XACT container DirectXTK
//    reads, and the extension is spelt once, below, rather than in the loader
//    that asks for a bank. docs/survey/2026-08-26.md 6 leaves the format
//    question open and this is the side of the cut it left it on.
//  - THE LIFETIME RULE. DirectXTK requires the AudioEngine to outlive every
//    WaveBank and SoundEffectInstance, because their destructors unregister
//    themselves from it. That used to be a paragraph in engine/app/
//    application.h's member list, keeping a DirectXTK ordering constraint in
//    the shell of a 2D engine. It is Impl's declaration order now, six lines
//    from the library that requires it.
//  - THE DEVICE-LOSS ANSWER, WHICH IS STILL TO IGNORE IT. AudioEngine::Update
//    answers false when the audio device has gone, and DirectXTK's documented
//    reply is to check IsCriticalError() and Reset(). Nothing in this tree has
//    ever done that, and this port deliberately did not start: the ignore is
//    below, where the platform is, instead of in the frame loop where it was.

using namespace DirectX;

namespace labrador
{
	namespace
	{
		// The one place the container's spelling appears.
		std::string container_path(const std::string& directory,
			const std::string& name)
		{
			return directory + name + ".xwb";
		}
	}

	struct AudioDevice::Impl
	{
		// DECLARATION ORDER IS LOAD-BEARING. Members destruct in reverse
		// declaration order, so the engine is declared first and dies last -
		// after every bank and every voice that unregisters itself from it.
		std::unique_ptr<AudioEngine> engine;

		std::vector<std::unique_ptr<WaveBank>> banks;
		std::vector<std::unique_ptr<SoundEffectInstance>> voices;

		WaveBank& bank(WaveBankHandle handle)
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->banks.size())
			{
				throw std::out_of_range(
					"A wave bank was reached through an unresolved handle.");
			}
			return *this->banks[index];
		}

		SoundEffectInstance& voice(VoiceHandle handle)
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->voices.size())
			{
				throw std::out_of_range(
					"A voice was reached through an unresolved handle.");
			}
			return *this->voices[index];
		}
	};

	AudioDevice::AudioDevice() : impl_(std::make_unique<Impl>())
	{
		AUDIO_ENGINE_FLAGS flags = AudioEngine_Default;
#ifdef _DEBUG
		flags |= AudioEngine_Debug;
#endif
		this->impl_->engine = std::make_unique<AudioEngine>(flags);
	}

	AudioDevice::~AudioDevice() = default;
	AudioDevice::AudioDevice(AudioDevice&&) noexcept = default;
	AudioDevice& AudioDevice::operator=(AudioDevice&&) noexcept = default;

	AudioDevice::WaveBankHandle AudioDevice::open_wave_bank(
		const std::string& directory, const std::string& name,
		const std::vector<std::string>& wave_names)
	{
		const std::string path = container_path(directory, name);

		std::unique_ptr<WaveBank> bank;
		try
		{
			bank = std::make_unique<WaveBank>(this->impl_->engine.get(),
				std::wstring(path.begin(), path.end()).c_str());
		}
		catch (const std::exception&)
		{
			// DirectXTK's what() is "WaveBank" and nothing else - T6 wants the
			// file. The type matters as much as the text: a caller telling a
			// missing optional bank from a broken one catches this and not the
			// throw below, and std::out_of_range is not a std::runtime_error.
			throw std::runtime_error("Failed to open wave bank: " + path);
		}

		// EVERY NAME THE DEFINITION LISTS, CHECKED HERE, WHICH IS EARLIER AND
		// STRICTER THAN IT USED TO BE. A wave the definition named and the
		// container does not hold used to surface in one of two places
		// depending on content that had nothing to do with it: at
		// WaveBank::CreateInstance, if some effect instance happened to name
		// it, and otherwise not until a resolve_wave that might never come.
		// Both are the same content bug and it is found once, at load, naming
		// the wave and the file (T6).
		for (const std::string& wave_name : wave_names)
		{
			if (bank->Find(wave_name.c_str()) < 0)
			{
				throw std::out_of_range("Wave bank '" + path +
					"' has no wave named '" + wave_name + "'.");
			}
		}

		this->impl_->banks.push_back(std::move(bank));
		return WaveBankHandle(static_cast<int>(this->impl_->banks.size()) - 1);
	}

	int AudioDevice::wave_index(WaveBankHandle bank,
		const std::string& wave_name) const
	{
		// WaveBank::Find already answers -1 for a name it does not have, which
		// is this method's contract - so the seam borrowed the spelling rather
		// than inventing one for a backend to translate into.
		return this->impl_->bank(bank).Find(wave_name.c_str());
	}

	AudioDevice::VoiceHandle AudioDevice::create_voice(WaveBankHandle bank,
		int wave_index)
	{
		std::unique_ptr<SoundEffectInstance> voice =
			this->impl_->bank(bank).CreateInstance(
				static_cast<unsigned int>(wave_index));
		if (!voice)
		{
			// CreateInstance answers null rather than throwing. open_wave_bank
			// checked every name the definition listed, so reaching this means
			// an index that came from somewhere else.
			throw std::out_of_range(
				"A voice was asked for a wave the bank does not hold.");
		}

		this->impl_->voices.push_back(std::move(voice));
		return VoiceHandle(static_cast<int>(this->impl_->voices.size()) - 1);
	}

	void AudioDevice::play_wave(WaveBankHandle bank, int wave_index,
		float volume, float pitch, float pan) const
	{
		this->impl_->bank(bank).Play(static_cast<unsigned int>(wave_index),
			volume, pitch, pan);
	}

	void AudioDevice::play_voice(VoiceHandle voice, bool loop, float volume,
		float pitch, float pan) const
	{
		SoundEffectInstance& instance = this->impl_->voice(voice);
		instance.SetVolume(volume);
		instance.SetPitch(pitch);
		instance.SetPan(pan);
		instance.Play(loop);
	}

	void AudioDevice::stop_voice(VoiceHandle voice, bool immediate) const
	{
		this->impl_->voice(voice).Stop(immediate);
	}

	void AudioDevice::pause_voice(VoiceHandle voice) const
	{
		this->impl_->voice(voice).Pause();
	}

	void AudioDevice::resume_voice(VoiceHandle voice) const
	{
		this->impl_->voice(voice).Resume();
	}

	void AudioDevice::set_voice_volume(VoiceHandle voice, float volume) const
	{
		this->impl_->voice(voice).SetVolume(volume);
	}

	void AudioDevice::set_voice_pitch(VoiceHandle voice, float pitch) const
	{
		this->impl_->voice(voice).SetPitch(pitch);
	}

	void AudioDevice::set_voice_pan(VoiceHandle voice, float pan) const
	{
		this->impl_->voice(voice).SetPan(pan);
	}

	SoundState AudioDevice::voice_state(VoiceHandle voice) const
	{
		switch (this->impl_->voice(voice).GetState())
		{
		case DirectX::PLAYING:
			return SoundState::playing;
		case DirectX::PAUSED:
			return SoundState::paused;
		case DirectX::STOPPED:
		default:
			return SoundState::stopped;
		}
	}

	bool AudioDevice::is_voice_looping(VoiceHandle voice) const
	{
		return this->impl_->voice(voice).IsLooped();
	}

	void AudioDevice::update()
	{
		// The answer is false when the audio device has gone, and the reply
		// DirectXTK documents is IsCriticalError() then Reset(). Nothing here
		// has ever made that reply, and the ignore is deliberate rather than
		// forgotten - it just used to sit in Application::update, where a
		// reader could not tell which of the two it was.
		std::ignore = this->impl_->engine->Update();
	}

	void AudioDevice::suspend()
	{
		this->impl_->engine->Suspend();
	}

	void AudioDevice::resume()
	{
		this->impl_->engine->Resume();
	}
}

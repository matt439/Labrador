#include "engine/audio/audio_device.h"
#include "engine/audio/wave_data_checks.h"
#include "engine/audio/xaudio2/device_recovery.h"
#include "engine/audio/xaudio2/wave_bank_policy.h"
#include "engine/audio/wave_data.h"
#include "engine/core/file_path.h"

#include <Audio.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// The XAudio2 backend, through DirectXTK, and the only file in this repository
// that includes <Audio.h>. Nothing above audio_device.h names an audio API,
// so a game never names a Microsoft library to ask what its music is doing,
// and cmake/check_engine_includes.cmake fails the build for a file outside
// this folder that reaches for one of ours.
//
// WHAT THIS FILE OWNS THAT THE SEAM DOES NOT. Three things, and they are the
// three the seam declines to decide:
//
//  - THE CONTAINER. A wave bank is an .xwb here, an XACT container DirectXTK
//    reads, and the extension is spelt once, below, rather than in the loader
//    that asks for a bank. The engine's own reader, engine/audio/xwb_file.h,
//    is for backends whose API has none; this one keeps DirectXTK's, because
//    XAudio2 plays ADPCM and xWMA as they are stored and the engine's reader
//    would decode the first to four times its size and refuse the second.
//    A memory bank contains decoded PCM instead; each wave is an owning
//    SoundEffect, independent of the caller's sample storage.
//  - THE LIFETIME RULE. DirectXTK requires the AudioEngine to outlive every
//    WaveBank, SoundEffect and SoundEffectInstance, because their destructors
//    unregister themselves from it. It is Impl's declaration order, in this file beside
//    the library that requires it, rather than a paragraph in the shell's
//    member list - which is where a DirectXTK ordering constraint must not
//    sit.
//  - DEVICE LOSS. Silent output is retried once a second while active. Reset
//    preserves banks and instance objects but drops playback, so games decide
//    which persistent sounds to start again. A suspended engine is not reset:
//    DirectXTK's new graph would not remember the old graph's StopEngine().
//
// wave_bank_policy.h holds the rest of what this backend decides around a
// WaveBank - which index it holds, how a failed open is reported, and the
// one-shot dropped while suspended - as templates a test can reach.

using namespace DirectX;

namespace labrador
{
	namespace
	{
		// The one place this backend spells the container; webaudio/ spells
		// it the same way in its own copy.
		std::string container_path(const std::string& directory,
			const std::string& name)
		{
			return directory + name + ".xwb";
		}
	}

	struct AudioDevice::Impl
	{
		struct Bank
		{
			std::unique_ptr<WaveBank> container;
			std::vector<std::string> names;
			std::vector<std::unique_ptr<SoundEffect>> memory_waves;

			bool holds_wave(int wave_index) const
			{
				return this->container != nullptr
					? detail::holds_wave<WAVEFORMATEX>(*this->container, wave_index)
					: wave_index >= 0 && static_cast<size_t>(wave_index) < this->memory_waves.size();
			}
		};

		// DECLARATION ORDER IS LOAD-BEARING. Members destruct in reverse
		// declaration order, so the engine is declared first and dies last -
		// after every bank and every voice that unregisters itself from it.
		std::unique_ptr<AudioEngine> engine;
		detail::DeviceRecovery recovery;

		std::vector<Bank> banks;
		std::vector<std::unique_ptr<SoundEffectInstance>> voices;

		Bank& bank(WaveBankHandle handle)
		{
			const size_t index = static_cast<size_t>(handle.index());
			if (!handle.valid() || index >= this->banks.size())
			{
				throw std::out_of_range(
					"A wave bank was reached through an unresolved handle.");
			}
			return this->banks[index];
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
		// XAudio2's debug layer, only under a debugger. DirectXTK sets it to
		// break on every error, and a break instruction with nobody attached
		// ends the process: a debug build on a machine with no audio endpoint
		// - a build runner, a headless session, a desk with nothing plugged
		// in - stopped at this line instead of carrying on silent, as a
		// release build does. Under a debugger the break is the point.
		if (IsDebuggerPresent())
		{
			flags |= AudioEngine_Debug;
		}
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

		// The open's failures become the runtime_error naming the file, and a
		// streaming bank is refused, in wave_bank_policy.h. The type matters as
		// much as the text: a caller telling a missing optional bank from a
		// broken one catches that and not the throw below, and
		// std::out_of_range is not a std::runtime_error.
		AudioEngine* const engine = this->impl_->engine.get();
		std::unique_ptr<WaveBank> bank = detail::open_wave_bank<WaveBank>(
			path, [engine, &path]()
			{
				return std::make_unique<WaveBank>(engine,
					path_from_utf8(path).c_str());
			});

		// EVERY NAME THE DEFINITION LISTS, CHECKED HERE, WHICH IS THE EARLIEST
		// AND STRICTEST PLACE FOR IT. Left to the play path, a wave the
		// definition names and the container does not hold surfaces in one of
		// two places depending on content that has nothing to do with it: at
		// WaveBank::CreateInstance, if some effect instance happens to name it,
		// and otherwise not until a resolve_wave that may never come. Both are
		// the same content bug and it is found once, at load, naming the wave
		// and the file (T6).
		for (const std::string& wave_name : wave_names)
		{
			if (bank->Find(wave_name.c_str()) < 0)
			{
				throw std::out_of_range("Wave bank '" + path +
					"' has no wave named '" + wave_name + "'.");
			}
		}

		Impl::Bank resource;
		resource.container = std::move(bank);
		this->impl_->banks.push_back(std::move(resource));
		return WaveBankHandle(static_cast<int>(this->impl_->banks.size()) - 1);
	}

	AudioDevice::WaveBankHandle AudioDevice::open_memory_bank(WaveBankData data)
	{
		detail::validate_wave_bank_data(data);
		if (this->impl_->banks.size() >= static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			throw std::length_error("Memory wave bank '" + data.name + "' exceeds the bank handle range.");
		}

		// Publish only after every wave has been copied into DirectXTK-owned
		// storage. Failed construction leaves the bank/voice tables unchanged.
		Impl::Bank bank;
		bank.names.reserve(data.waves.size());
		bank.memory_waves.reserve(data.waves.size());
		for (const WaveData& wave : data.waves)
		{
			const size_t sample_bytes = wave.samples.size() * sizeof(int16_t);
			std::unique_ptr<uint8_t[]> storage(new uint8_t[sizeof(WAVEFORMATEX) + sample_bytes]);
			WAVEFORMATEX* const format = ::new (storage.get()) WAVEFORMATEX{};
			format->wFormatTag = WAVE_FORMAT_PCM;
			format->nChannels = static_cast<WORD>(wave.channels);
			format->nSamplesPerSec = static_cast<DWORD>(wave.sample_rate);
			format->nBlockAlign = static_cast<WORD>(wave.channels * sizeof(int16_t));
			format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
			format->wBitsPerSample = 16;
			uint8_t* const samples = storage.get() + sizeof(WAVEFORMATEX);
			std::memcpy(samples, wave.samples.data(), sample_bytes);
			try
			{
				std::unique_ptr<SoundEffect> sound = std::make_unique<SoundEffect>(
					this->impl_->engine.get(), storage, format, samples, sample_bytes,
					wave.loop_length == 0 ? 0 : wave.loop_start, wave.loop_length);
				bank.names.push_back(wave.name);
				bank.memory_waves.push_back(std::move(sound));
			}
			catch (const std::bad_alloc&)
			{
				throw;
			}
			catch (const std::exception& unread)
			{
				throw std::runtime_error("Failed to open memory wave bank '" + data.name +
					"' wave '" + wave.name + "' (" + unread.what() + ")");
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
		const Impl::Bank& resource = this->impl_->bank(bank);
		if (resource.container != nullptr)
		{
			return resource.container->Find(wave_name.c_str());
		}
		const std::vector<std::string>::const_iterator found =
			std::find(resource.names.begin(), resource.names.end(), wave_name);
		return found == resource.names.end() ? -1 : static_cast<int>(found - resource.names.begin());
	}

	AudioDevice::VoiceHandle AudioDevice::create_voice(WaveBankHandle bank,
		int wave_index)
	{
		// The same check and throw as the other two backends, before the
		// unsigned cast would turn a negative index into a large one.
		// CreateInstance answers null only for an index past the end, which
		// this has refused. open_wave_bank checked every name the definition
		// listed, so failing here means an index that came from somewhere
		// else, such as another bank.
		Impl::Bank& wave_bank = this->impl_->bank(bank);
		if (!wave_bank.holds_wave(wave_index))
		{
			throw std::out_of_range(
				"A voice was asked for a wave the bank does not hold.");
		}

		std::unique_ptr<SoundEffectInstance> voice = wave_bank.container != nullptr
			? wave_bank.container->CreateInstance(static_cast<unsigned int>(wave_index))
			: wave_bank.memory_waves[static_cast<size_t>(wave_index)]->CreateInstance();

		this->impl_->voices.push_back(std::move(voice));
		return VoiceHandle(static_cast<int>(this->impl_->voices.size()) - 1);
	}

	void AudioDevice::play_wave(WaveBankHandle bank, int wave_index,
		float volume, float pitch, float pan) const
	{
		Impl::Bank& resource = this->impl_->bank(bank);
		if (resource.container != nullptr)
		{
			detail::play_one_shot<WAVEFORMATEX>(*resource.container,
				this->impl_->recovery, wave_index, volume, pitch, pan);
			return;
		}
		if (!resource.holds_wave(wave_index))
		{
			throw std::out_of_range("A wave was played that the bank does not hold.");
		}
		if (!this->impl_->recovery.suspended())
		{
			resource.memory_waves[static_cast<size_t>(wave_index)]->Play(volume, pitch, pan);
		}
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
		this->impl_->recovery.update(*this->impl_->engine,
			detail::DeviceRecovery::Clock::now());
	}

	void AudioDevice::suspend()
	{
		this->impl_->recovery.suspend();
		this->impl_->engine->Suspend();
	}

	void AudioDevice::resume()
	{
		this->impl_->engine->Resume();
		this->impl_->recovery.resume();
	}
}

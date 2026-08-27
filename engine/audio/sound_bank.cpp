#include "engine/audio/sound_bank.h"

#include "engine/audio/audio_device.h"
#include "engine/math/scalar.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace mattmath;

namespace labrador
{
	SoundBank::SoundBank(const AudioDevice* device,
		AudioDevice::WaveBankHandle wave_bank,
		Registry<SoundEffect> effects) :
		device_(device),
		wave_bank_(wave_bank),
		effects_(std::move(effects))
	{

	}

	std::unique_ptr<SoundBank> SoundBank::silent()
	{
		return std::unique_ptr<SoundBank>(new SoundBank(nullptr,
			AudioDevice::WaveBankHandle(), Registry<SoundEffect>("SoundEffect")));
	}

	bool SoundBank::audible() const
	{
		return this->device_ != nullptr;
	}

	SoundBank::WaveHandle SoundBank::resolve_wave(
		const std::string& wave_name) const
	{
		if (!this->audible())
		{
			// Any name, and index 0 rather than an unresolved handle: every
			// play below rejects an unresolved handle in every build now, and a
			// bank with no content must not reject a name.
			return WaveHandle(0);
		}

		// The seam answers -1 for a name a bank does not have, which is also
		// what an unresolved Handle holds - so this has to be caught here
		// rather than handed on as a handle that looks fine until it is read.
		const int index =
			this->device_->wave_index(this->wave_bank_, wave_name);
		if (index < 0)
		{
			throw std::out_of_range(
				"Wave '" + wave_name + "' is not in this sound bank.");
		}
		return WaveHandle(index);
	}

	SoundBank::EffectHandle SoundBank::resolve_effect(
		const std::string& effect_name) const
	{
		if (!this->audible())
		{
			return EffectHandle(0);
		}
		return this->effects_.resolve(effect_name);
	}

	void SoundBank::play_wave(WaveHandle wave, float volume, float pitch,
		float pan) const
	{
		// THE CLAMP IS FIRST AND THAT IS THE POINT. Folding a level back into
		// range is arithmetic this module owns - it is not XAudio2's and it is
		// not any backend's, and it is the same kind of engine-side decision as
		// the glyph walk in render/font.h. It used to sit below the test for
		// content, where a tree that cannot build an audible bank - which this
		// one could not - never reached it at all.
		clamp_levels(volume, pitch, pan);

		if (!wave.valid())
		{
			throw std::out_of_range(
				"A wave was played through an unresolved handle.");
		}

		if (!this->audible())
		{
			return;
		}

		this->device_->play_wave(this->wave_bank_, wave.index(), volume, pitch,
			pan);
	}

	void SoundBank::play_effect(EffectHandle effect, bool loop, float volume,
		float pitch, float pan) const
	{
		clamp_levels(volume, pitch, pan);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->play_voice(voice, loop, volume, pitch, pan);
	}
	void SoundBank::stop_effect(EffectHandle effect, bool immediate) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->stop_voice(voice, immediate);
	}
	void SoundBank::pause_effect(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->pause_voice(voice);
	}
	void SoundBank::resume_effect(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->resume_voice(voice);
	}
	void SoundBank::set_effect_volume(EffectHandle effect, float volume) const
	{
		volume = clamp(volume, 0.0f, 1.0f);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->set_voice_volume(voice, volume);
	}
	void SoundBank::set_effect_pitch(EffectHandle effect, float pitch) const
	{
		pitch = clamp(pitch, -1.0f, 1.0f);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->set_voice_pitch(voice, pitch);
	}
	void SoundBank::set_effect_pan(EffectHandle effect, float pan) const
	{
		pan = clamp(pan, -1.0f, 1.0f);
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return;
		}

		this->device_->set_voice_pan(voice, pan);
	}
	SoundState SoundBank::effect_state(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return SoundState::stopped;
		}

		return this->device_->voice_state(voice);
	}
	bool SoundBank::is_effect_looping(EffectHandle effect) const
	{
		const AudioDevice::VoiceHandle voice = this->voice(effect);
		if (!this->audible())
		{
			return false;
		}

		return this->device_->is_voice_looping(voice);
	}

	AudioDevice::VoiceHandle SoundBank::voice(EffectHandle effect) const
	{
		if (!effect.valid())
		{
			// Above the check for content, deliberately: a handle nobody
			// resolved is a program's mistake rather than a missing file, and
			// the two builds should not disagree about whether to say so.
			throw std::out_of_range(
				"A sound effect was reached through an unresolved handle.");
		}

		if (!this->audible())
		{
			return AudioDevice::VoiceHandle();
		}

		// A bounds check and an indexed load, and the registry's own throw
		// naming the effect if the handle came from another table.
		return this->effects_.get(effect)->voice;
	}

	void SoundBank::clamp_levels(float& volume, float& pitch, float& pan)
	{
		volume = clamp(volume, 0.0f, 1.0f);
		pitch = clamp(pitch, -1.0f, 1.0f);
		pan = clamp(pan, -1.0f, 1.0f);
	}
}

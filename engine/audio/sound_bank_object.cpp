#include "engine/audio/sound_bank_object.h"

#include <stdexcept>
#include <string>

using namespace DirectX;

namespace labrador
{
	namespace
	{
		// Every path into this class goes through the table, and there are two
		// ways to arrive without one: default-construct, or hand the
		// constructor a null. Both used to be a null dereference in a client's
		// frame. This is the rest of the engine's answer instead - Registry
		// never returns nullptr either, it says what was missing and stops
		// (T6) - and it is one function because the alternative is the same
		// sentence written three times and drifting apart.
		const AudioResources* table_of(const AudioResources* audio_resources)
		{
			if (audio_resources == nullptr)
			{
				throw std::logic_error("SoundBankObject has no AudioResources "
					"to reach a bank through. A default-constructed one never "
					"had any and there is no setter for it: the constructor "
					"taking a bank name is the only thing that fills it in.");
			}
			return audio_resources;
		}
	}

	SoundBankObject::SoundBankObject(const std::string& sound_bank_name,
	                                 const AudioResources* audio_resources) :
		sound_bank_(table_of(audio_resources)->resolve_sound_bank(
			sound_bank_name)),
		audio_resources_(audio_resources)

	{

	}
	SoundBank* SoundBankObject::sound_bank() const
	{
		return table_of(this->audio_resources_)->sound_bank(this->sound_bank_);
	}

	SoundBank::WaveHandle SoundBankObject::resolve_wave(
		const std::string& wave_name) const
	{
		return this->sound_bank()->resolve_wave(wave_name);
	}
	SoundBank::EffectHandle SoundBankObject::resolve_effect(
		const std::string& effect_name) const
	{
		return this->sound_bank()->resolve_effect(effect_name);
	}

	void SoundBankObject::play_wave(SoundBank::WaveHandle wave, float volume,
		float pitch, float pan) const
	{
		this->sound_bank()->play_wave(wave, volume, pitch, pan);
	}
	void SoundBankObject::play_effect(SoundBank::EffectHandle effect,
		bool loop, float volume, float pitch, float pan) const
	{
		this->sound_bank()->play_effect(effect, loop, volume, pitch, pan);
	}
	void SoundBankObject::stop_effect(SoundBank::EffectHandle effect,
		bool immediate) const
	{
		this->sound_bank()->stop_effect(effect, immediate);
	}
	void SoundBankObject::pause_effect(SoundBank::EffectHandle effect) const
	{
		this->sound_bank()->pause_effect(effect);
	}
	void SoundBankObject::resume_effect(SoundBank::EffectHandle effect) const
	{
		this->sound_bank()->resume_effect(effect);
	}
	void SoundBankObject::set_effect_volume(SoundBank::EffectHandle effect,
		float volume) const
	{
		this->sound_bank()->set_effect_volume(effect, volume);
	}
	void SoundBankObject::set_effect_pitch(SoundBank::EffectHandle effect,
		float pitch) const
	{
		this->sound_bank()->set_effect_pitch(effect, pitch);
	}
	void SoundBankObject::set_effect_pan(SoundBank::EffectHandle effect,
		float pan) const
	{
		this->sound_bank()->set_effect_pan(effect, pan);
	}
	SoundState SoundBankObject::effect_state(
		SoundBank::EffectHandle effect) const
	{
		return this->sound_bank()->effect_state(effect);
	}
	bool SoundBankObject::is_effect_looping(SoundBank::EffectHandle effect) const
	{
		return this->sound_bank()->is_effect_looping(effect);
	}

	void SoundBankObject::set_sound_bank(const std::string& sound_bank_name)
	{
		this->sound_bank_ = table_of(this->audio_resources_)
			->resolve_sound_bank(sound_bank_name);
	}
}

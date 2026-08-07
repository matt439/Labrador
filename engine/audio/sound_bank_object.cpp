#include "engine/audio/sound_bank_object.h"

using namespace DirectX;

SoundBankObject::SoundBankObject(const std::string& sound_bank_name,
                                 const AudioResources* audio_resources) :
	sound_bank_(audio_resources->resolve_sound_bank(sound_bank_name)),
	audio_resources_(audio_resources)

{

}
SoundBank* SoundBankObject::sound_bank() const
{
	return this->audio_resources_->sound_bank(this->sound_bank_);
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
	this->sound_bank_ = this->audio_resources_->resolve_sound_bank(
		sound_bank_name);
}

#include "engine/audio/sound_bank.h"
#include <stdexcept>

using namespace DirectX;
using namespace mattmath;

SoundBank::SoundBank(std::unique_ptr<WaveBank> wave_bank,
	Registry<SoundEffectInstance> instances) :
	wave_bank_(std::move(wave_bank)),
	sound_effect_instances_(std::move(instances))
{

}

SoundBank::WaveHandle SoundBank::resolve_wave(
	const std::string& wave_name) const
{
	// WaveBank::Find returns -1 for a name the bank does not have, which is
	// also what an unresolved Handle holds - so this has to be caught here
	// rather than handed on as a handle that looks fine until it is read.
	const int index = this->wave_bank_->Find(wave_name.c_str());
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
	return this->sound_effect_instances_.resolve(effect_name);
}

void SoundBank::play_wave(WaveHandle wave, float volume, float pitch,
	float pan) const
{
	if (!wave.valid())
	{
		throw std::out_of_range(
			"A wave was played through an unresolved handle.");
	}

	clamp_levels(volume, pitch, pan);
	this->wave_bank_->Play(static_cast<unsigned int>(wave.index()),
		volume, pitch, pan);
}

void SoundBank::play_effect(EffectHandle effect, bool loop, float volume,
	float pitch, float pan) const
{
	clamp_levels(volume, pitch, pan);
	SoundEffectInstance* instance = this->sound_effect_instance(effect);
	instance->SetVolume(volume);
	instance->SetPitch(pitch);
	instance->SetPan(pan);
	instance->Play(loop);
}
void SoundBank::stop_effect(EffectHandle effect, bool immediate) const
{
	this->sound_effect_instance(effect)->Stop(immediate);
}
void SoundBank::pause_effect(EffectHandle effect) const
{
	this->sound_effect_instance(effect)->Pause();
}
void SoundBank::resume_effect(EffectHandle effect) const
{
	this->sound_effect_instance(effect)->Resume();
}
void SoundBank::set_effect_volume(EffectHandle effect, float volume) const
{
	volume = clamp(volume, 0.0f, 1.0f);
	this->sound_effect_instance(effect)->SetVolume(volume);
}
void SoundBank::set_effect_pitch(EffectHandle effect, float pitch) const
{
	pitch = clamp(pitch, -1.0f, 1.0f);
	this->sound_effect_instance(effect)->SetPitch(pitch);
}
void SoundBank::set_effect_pan(EffectHandle effect, float pan) const
{
	pan = clamp(pan, -1.0f, 1.0f);
	this->sound_effect_instance(effect)->SetPan(pan);
}
SoundState SoundBank::effect_state(EffectHandle effect) const
{
	return this->sound_effect_instance(effect)->GetState();
}
bool SoundBank::is_effect_looping(EffectHandle effect) const
{
	return this->sound_effect_instance(effect)->IsLooped();
}
SoundEffectInstance* SoundBank::sound_effect_instance(
	EffectHandle effect) const
{
	// A bounds check and an indexed load, and the registry's own throw naming
	// the instance if the handle was never resolved.
	return this->sound_effect_instances_.get(effect);
}
void SoundBank::clamp_levels(float& volume, float& pitch, float& pan)
{
	volume = clamp(volume, 0.0f, 1.0f);
	pitch = clamp(pitch, -1.0f, 1.0f);
	pan = clamp(pan, -1.0f, 1.0f);
}

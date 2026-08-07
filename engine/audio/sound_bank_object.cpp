#include "engine/audio/sound_bank_object.h"

using namespace DirectX;

SoundBankObject::SoundBankObject(const std::string& sound_bank_name,
                                 const AudioResources* audio_resources) :
	_sound_bank(audio_resources->resolve_sound_bank(sound_bank_name)),
	_audio_resources(audio_resources)

{

}
SoundBank* SoundBankObject::get_sound_bank() const
{
	return this->_audio_resources->get_sound_bank(this->_sound_bank);
}
SoundBank* SoundBankObject::get_sb() const
{
	return this->get_sound_bank();
}

void SoundBankObject::play_wave(const std::string& wave_name, float volume, float pitch, float pan) const
{
	this->get_sb()->play_wave(wave_name, volume, pitch, pan);
}
void SoundBankObject::play_effect(const std::string& effect_name,
	bool loop, float volume, float pitch, float pan) const
{
	this->get_sb()->play_effect(effect_name, loop, volume, pitch, pan);
}
void SoundBankObject::stop_effect(const std::string& effect_name, bool immediate) const
{
	this->get_sb()->stop_effect(effect_name, immediate);
}
void SoundBankObject::pause_effect(const std::string& effect_name) const
{
	this->get_sb()->pause_effect(effect_name);
}
void SoundBankObject::resume_effect(const std::string& effect_name) const
{
	this->get_sb()->resume_effect(effect_name);
}
void SoundBankObject::set_effect_volume(const std::string& effect_name, float volume) const
{
	this->get_sb()->set_effect_volume(effect_name, volume);
}
void SoundBankObject::set_effect_pitch(const std::string& effect_name, float pitch) const
{
	this->get_sb()->set_effect_pitch(effect_name, pitch);
}
void SoundBankObject::set_effect_pan(const std::string& effect_name, float pan) const
{
	this->get_sb()->set_effect_pan(effect_name, pan);
}
SoundState SoundBankObject::get_effect_state(const std::string& effect_name) const
{
	return this->get_sb()->get_effect_state(effect_name);
}
bool SoundBankObject::is_effect_looping(const std::string& effect_name) const
{
	return this->get_sb()->is_effect_looping(effect_name);
}

SoundEffectInstance* SoundBankObject::get_sound_effect_instance(const std::string& instance_name) const
{
	return this->get_sb()->get_sound_effect_instance(instance_name);
}
SoundEffectInstance* SoundBankObject::get_sei(const std::string& instance_name) const
{
	return this->get_sound_effect_instance(instance_name);
}

void SoundBankObject::set_sound_bank(const std::string& sound_bank_name)
{
	this->_sound_bank = this->_audio_resources->resolve_sound_bank(
		sound_bank_name);
}
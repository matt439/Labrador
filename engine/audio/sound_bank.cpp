#include "engine/audio/sound_bank.h"
#include <stdexcept>

using namespace DirectX;
using namespace MattMath;

SoundBank::SoundBank(std::unique_ptr<WaveBank> wave_bank,
	std::map<std::string, std::unique_ptr<SoundEffectInstance>> instances) :
	_wave_bank(std::move(wave_bank)),
	_sound_effect_instances(std::move(instances))
{

}

void SoundBank::play_wave(const std::string& wave_name, float volume, float pitch, float pan) const
{
	clamp_levels(volume, pitch, pan);
	try
	{
		this->_wave_bank->Play(wave_name.c_str(), volume, pitch, pan);
	}
	catch (const std::out_of_range&)
	{
		throw std::out_of_range("Wave with name " + wave_name + " not found");
	}
}

void SoundBank::play_effect(const std::string& effect_name, bool loop, float volume,
	float pitch, float pan) const
{
	clamp_levels(volume, pitch, pan);
	SoundEffectInstance* sei = this->get_sound_effect_instance(effect_name);
	sei->SetVolume(volume);
	sei->SetPitch(pitch);
	sei->SetPan(pan);
	sei->Play(loop);
}
void SoundBank::stop_effect(const std::string& effect_name, bool immediate) const
{
	this->get_sound_effect_instance(effect_name)->Stop(immediate);
}
void SoundBank::pause_effect(const std::string& effect_name) const
{
	this->get_sound_effect_instance(effect_name)->Pause();
}
void SoundBank::resume_effect(const std::string& effect_name) const
{
	this->get_sound_effect_instance(effect_name)->Resume();
}
void SoundBank::set_effect_volume(const std::string& effect_name, float volume) const
{
	volume = clamp(volume, 0.0f, 1.0f);
	this->get_sound_effect_instance(effect_name)->SetVolume(volume);
}
void SoundBank::set_effect_pitch(const std::string& effect_name, float pitch) const
{
	pitch = clamp(pitch, -1.0f, 1.0f);
	this->get_sound_effect_instance(effect_name)->SetPitch(pitch);
}
void SoundBank::set_effect_pan(const std::string& effect_name, float pan) const
{
	pan = clamp(pan, -1.0f, 1.0f);
	this->get_sound_effect_instance(effect_name)->SetPan(pan);
}
SoundState SoundBank::get_effect_state(const std::string& effect_name) const
{
	return this->get_sound_effect_instance(effect_name)->GetState();
}
bool SoundBank::is_effect_looping(const std::string& effect_name) const
{
	return this->get_sound_effect_instance(effect_name)->IsLooped();
}
SoundEffectInstance* SoundBank::get_sound_effect_instance(const std::string& instance_name) const
{
	try
	{
		return this->_sound_effect_instances.at(instance_name).get();
	}
	catch (const std::out_of_range&)
	{
		throw std::out_of_range("SoundEffectInstance with name " + instance_name + " not found");
	}
}
SoundEffectInstance* SoundBank::get_sei(const std::string& instance_name) const
{
	return this->get_sound_effect_instance(instance_name);
}
void SoundBank::clamp_levels(float& volume, float& pitch, float& pan)
{
	volume = clamp(volume, 0.0f, 1.0f);
	pitch = clamp(pitch, -1.0f, 1.0f);
	pan = clamp(pan, -1.0f, 1.0f);
}
void SoundBank::reset_all_instances()
{
	for (auto& sei : this->_sound_effect_instances)
	{
		sei.second->Stop(true);
		sei.second.reset();
	}
}
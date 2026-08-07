#ifndef SOUNDBANKOBJECT_H
#define SOUNDBANKOBJECT_H

#include "engine/audio/sound_bank.h"
#include "engine/audio/audio_resources.h"
#include <string>

class SoundBankObject
{
public:
	SoundBankObject() = default;
	SoundBankObject(const std::string& sound_bank_name,
		const AudioResources* audio_resources);
protected:
	SoundBank* get_sound_bank() const;
	SoundBank* get_sb() const;

	void play_wave(const std::string& wave_name,
		float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f) const;
	void play_effect(const std::string& effect_name,
		bool loop = false, float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f) const;
	void stop_effect(const std::string& effect_name, bool immediate = false) const;
	void pause_effect(const std::string& effect_name) const;
	void resume_effect(const std::string& effect_name) const;
	void set_effect_volume(const std::string& effect_name, float volume) const;
	void set_effect_pitch(const std::string& effect_name, float pitch) const;
	void set_effect_pan(const std::string& effect_name, float pan) const;
	DirectX::SoundState get_effect_state(const std::string& effect_name) const;
	bool is_effect_looping(const std::string& effect_name) const;

	DirectX::SoundEffectInstance* get_sound_effect_instance(const std::string& instance_name) const;
	DirectX::SoundEffectInstance* get_sei(const std::string& instance_name) const;
	
	void set_sound_bank_name(const std::string& sound_bank_name);
private:
	std::string _sound_bank_name = "";
	const AudioResources* _audio_resources = nullptr;
};
#endif // !SOUNDBANKOBJECT_H

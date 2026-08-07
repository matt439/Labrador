#ifndef SOUNDBANK_H
#define SOUNDBANK_H

#include <Audio.h>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include "engine/math/matt_math.h"

class SoundBank
{
public:
	// Built by sound_bank_loader (engine/assets/) from an already-parsed
	// definition: the bank plays what it was handed, it does not read files.
	SoundBank(std::unique_ptr<DirectX::WaveBank> wave_bank,
		std::map<std::string,
			std::unique_ptr<DirectX::SoundEffectInstance>> instances);

	void play_wave(const std::string& wave_name, float volume = 1.0f,
		float pitch = 0.0f, float pan = 0.0f) const;

	void play_effect(const std::string& effect_name, bool loop = false, float volume = 1.0f,
		float pitch = 0.0f, float pan = 0.0f) const;
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

	void reset_all_instances();
private:
	std::unique_ptr<DirectX::WaveBank> _wave_bank = nullptr;
	std::map<std::string, std::unique_ptr<DirectX::SoundEffectInstance>> _sound_effect_instances;

	static void clamp_levels(float& volume, float& pitch, float& pan);
};

#endif // !SOUNDBANK_H

#ifndef SOUNDBANK_H
#define SOUNDBANK_H

#include <Audio.h>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <rapidjson/document.h>
#include "engine/math/matt_math.h"

class SoundBank
{
public:
	SoundBank() = default;
	explicit SoundBank(std::unique_ptr<DirectX::WaveBank> wave_bank);

	void load_from_json(const char* json_path);

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
	std::vector<std::string> _wave_names;
	std::map<std::string, std::unique_ptr<DirectX::SoundEffectInstance>> _sound_effect_instances;

	static std::vector<std::string>
		decode_wave_names_json(const rapidjson::Value& json);
	std::map<std::string, std::unique_ptr<DirectX::SoundEffectInstance>>
		decode_sound_effect_instances_json(const rapidjson::Value& json,
			bool create_effect_instance_for_each_wave) const;

	static void clamp_levels(float& volume, float& pitch, float& pan);
};

#endif // !SOUNDBANK_H

#include "engine/assets/sound_bank_loader.h"
#include "engine/assets/json_loader.h"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DirectX;
using namespace rapidjson;

namespace
{
	std::vector<std::string> decode_wave_names(const Value& json)
	{
		std::vector<std::string> wave_names;
		for (auto& wave : json.GetArray())
		{
			wave_names.push_back(wave.GetString());
		}
		return wave_names;
	}

	std::unique_ptr<SoundEffectInstance> create_instance(
		WaveBank& wave_bank, const std::string& wave_name)
	{
		try
		{
			return wave_bank.CreateInstance(wave_name.c_str());
		}
		catch (const std::out_of_range&)
		{
			throw std::out_of_range("Wave with name " + wave_name + " not found");
		}
	}

	std::map<std::string, std::unique_ptr<SoundEffectInstance>> decode_instances(
		WaveBank& wave_bank, const Value& waves_json,
		const Value& instances_json, bool instance_for_each_wave)
	{
		std::map<std::string, std::unique_ptr<SoundEffectInstance>> instances;

		if (instance_for_each_wave)
		{
			for (const std::string& wave : decode_wave_names(waves_json))
			{
				instances[wave] = create_instance(wave_bank, wave);
			}
		}

		for (auto& effect : instances_json.GetArray())
		{
			std::string name = effect["name"].GetString();
			std::string wave = effect["wave"].GetString();

			if (instances.find(name) != instances.end())
			{
				throw std::runtime_error(
					"SoundEffectInstance with name " + name + " already exists");
			}
			instances[name] = create_instance(wave_bank, wave);
		}
		return instances;
	}
}

std::unique_ptr<SoundBank> sound_bank_loader::load(const char* json_path,
	std::unique_ptr<WaveBank> wave_bank)
{
	const Document doc = json_loader::parse_file(json_path);

	auto instances = decode_instances(*wave_bank,
		doc["waves"],
		doc["sound_effect_instances"],
		doc["create_effect_instance_for_each_wave"].GetBool());

	return std::make_unique<SoundBank>(std::move(wave_bank),
		std::move(instances));
}

#include "engine/assets/sound_bank_loader.h"
#include "engine/assets/json.h"
#include <memory>
#include <stdexcept>
#include <string>

using namespace DirectX;

namespace labrador
{
	namespace
	{
		std::unique_ptr<SoundEffectInstance> create_instance(
			WaveBank& wave_bank, const std::string& wave_name,
			const std::string& source_path)
		{
			// WaveBank::CreateInstance answers null for a name the bank does not
			// hold - it does not throw. The catch that used to be here had
			// therefore never run, and a definition naming a missing wave
			// registered a null instance that failed later, somewhere else, at
			// the first attempt to play it.
			std::unique_ptr<SoundEffectInstance> instance =
				wave_bank.CreateInstance(wave_name.c_str());
			if (!instance)
			{
				throw std::out_of_range("'" + source_path +
					"': the wave bank has no wave named '" + wave_name + "'");
			}
			return instance;
		}

		Registry<SoundEffectInstance> decode_instances(WaveBank& wave_bank,
			const JsonValue& root, const std::string& source_path)
		{
			Registry<SoundEffectInstance> instances("SoundEffectInstance");

			const JsonValue waves = root.array("waves");
			if (root.boolean("create_effect_instance_for_each_wave"))
			{
				for (size_t index = 0; index < waves.size(); ++index)
				{
					const std::string wave = waves.at(index).as_string();
					instances.add(wave,
						create_instance(wave_bank, wave, source_path));
				}
			}

			const JsonValue effects = root.array("sound_effect_instances");
			for (size_t index = 0; index < effects.size(); ++index)
			{
				const JsonValue effect = effects.at(index);
				const std::string name = effect.string("name");

				// Registry::add refills a name's slot rather than rejecting it, so
				// the duplicate check has to happen here: two definitions claiming
				// one name is a content bug, not an overwrite.
				if (instances.contains(name))
				{
					throw std::runtime_error("'" + source_path +
						"': two sound effect instances are named '" + name + "'");
				}
				instances.add(name, create_instance(wave_bank,
					effect.string("wave"), source_path));
			}
			return instances;
		}
	}

	std::unique_ptr<SoundBank> read_sound_bank(const char* json_path,
		std::unique_ptr<WaveBank> wave_bank)
	{
		const JsonDocument document = read_json_file(json_path);

		auto instances = decode_instances(*wave_bank, document.root(),
			document.source_path());

		return std::make_unique<SoundBank>(std::move(wave_bank),
			std::move(instances));
	}
}

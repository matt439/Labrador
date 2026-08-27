#include "engine/assets/sound_bank_loader.h"

#include "engine/assets/json.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/sound_bank.h"
#include "engine/core/registry.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace labrador
{
	namespace
	{
		bool holds(const std::vector<std::string>& names,
			const std::string& name)
		{
			return std::find(names.begin(), names.end(), name) != names.end();
		}

		bool names_effect(const std::vector<SoundBankDefinition::Effect>& effects,
			const std::string& name)
		{
			for (const SoundBankDefinition::Effect& effect : effects)
			{
				if (effect.name == name)
				{
					return true;
				}
			}
			return false;
		}
	}

	SoundBankDefinition read_sound_bank_definition(const char* json_path)
	{
		const JsonDocument document = read_json_file(json_path);
		const JsonValue root = document.root();

		SoundBankDefinition definition;
		definition.source_path = document.source_path();

		// The declared wave list, in order and without repeats. A repeat used
		// to be absorbed by Registry::add refilling the name's slot, which is
		// invisible; a list is not a registry, so it is dropped here instead
		// and the result is the same one effect.
		const JsonValue waves = root.array("waves");
		for (size_t index = 0; index < waves.size(); ++index)
		{
			const std::string wave = waves.at(index).as_string();
			if (!holds(definition.waves, wave))
			{
				definition.waves.push_back(wave);
			}
		}

		if (root.boolean("create_effect_instance_for_each_wave"))
		{
			for (const std::string& wave : definition.waves)
			{
				definition.effects.push_back(
					SoundBankDefinition::Effect{ wave, wave });
			}
		}

		const JsonValue effects = root.array("sound_effect_instances");
		for (size_t index = 0; index < effects.size(); ++index)
		{
			const JsonValue effect = effects.at(index);
			const std::string name = effect.string("name");

			// Two definitions claiming one name is a content bug rather than an
			// overwrite, and this is still the line that says so - it used to
			// have to, because Registry::add refills a name's slot rather than
			// rejecting it, and it still does because a silent second voice on
			// one name is the same defect whatever holds them.
			if (names_effect(definition.effects, name))
			{
				throw std::runtime_error("'" + definition.source_path +
					"': two sound effect instances are named '" + name + "'");
			}

			const std::string wave = effect.string("wave");
			definition.effects.push_back(
				SoundBankDefinition::Effect{ name, wave });

			// An effect may name a wave the `waves` array did not list, and
			// that used to be how such a wave reached the bank at all. It has
			// to be in this list now: the list is what a backend with no
			// container answers wave_index out of, so a wave nothing declares
			// is a wave nothing can find.
			if (!holds(definition.waves, wave))
			{
				definition.waves.push_back(wave);
			}
		}

		return definition;
	}

	std::unique_ptr<SoundBank> build_sound_bank(AudioDevice* device,
		AudioDevice::WaveBankHandle wave_bank,
		const SoundBankDefinition& definition)
	{
		Registry<SoundBank::SoundEffect> effects("SoundEffect");

		for (const SoundBankDefinition::Effect& effect : definition.effects)
		{
			const int wave = device->wave_index(wave_bank, effect.wave);
			if (wave < 0)
			{
				// Unreachable through a backend that checked the definition's
				// wave list when it opened the bank, which both of this tree's
				// do. It is here because "the seam answers -1" is the contract
				// and a caller that ignores it is the defect this class exists
				// to make loud (T6).
				throw std::out_of_range("'" + definition.source_path +
					"': the wave bank has no wave named '" + effect.wave + "'");
			}

			effects.add(effect.name,
				std::make_unique<SoundBank::SoundEffect>(
					SoundBank::SoundEffect{ device->create_voice(wave_bank,
						wave) }));
		}

		return std::make_unique<SoundBank>(device, wave_bank,
			std::move(effects));
	}
}

#include "engine/audio/audio_resources.h"
#include "engine/math/scalar.h"

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace labrador;

namespace labrador
{
	AudioResources::SoundBankHandle AudioResources::resolve_sound_bank(
		const std::string& sound_bank_name) const
	{
		return this->sound_banks_.resolve(sound_bank_name);
	}

	SoundBank* AudioResources::sound_bank(SoundBankHandle sound_bank) const
	{
		return this->sound_banks_.get(sound_bank);
	}

	SoundBank* AudioResources::sound_bank(
		const std::string& sound_bank_name) const
	{
		return this->sound_banks_.get(sound_bank_name);
	}

	void AudioResources::add_sound_bank(const std::string& sound_bank_name,
		std::unique_ptr<SoundBank> sound_bank)
	{
		if (sound_bank != nullptr)
		{
			sound_bank->group_volumes_ = this->group_volumes_.get();
		}
		this->sound_banks_.add(sound_bank_name, std::move(sound_bank));

		// Re-adding a name refills its slot, so it is already listed.
		if (std::find(this->bank_order_.begin(), this->bank_order_.end(),
			sound_bank_name) == this->bank_order_.end())
		{
			this->bank_order_.push_back(sound_bank_name);
		}
	}

	AudioResources::SoundGroupHandle AudioResources::add_sound_group(
		const std::string& group_name)
	{
		if (this->group_indices_.find(group_name) !=
			this->group_indices_.end())
		{
			throw std::invalid_argument(
				"Sound group '" + group_name + "' was already added.");
		}

		const int index = static_cast<int>(this->group_volumes_->size());
		this->group_indices_.emplace(group_name, index);
		this->group_volumes_->push_back(1.0f);
		return SoundGroupHandle(index);
	}

	AudioResources::SoundGroupHandle AudioResources::resolve_sound_group(
		const std::string& group_name) const
	{
		const std::map<std::string, int>::const_iterator it =
			this->group_indices_.find(group_name);
		if (it == this->group_indices_.end())
		{
			throw std::out_of_range(
				"Sound group '" + group_name + "' was never added.");
		}
		return SoundGroupHandle(it->second);
	}

	void AudioResources::set_group_volume(SoundGroupHandle group, float volume)
	{
		// Clamp, then the handle: the same order as every SoundBank method.
		volume = clamp(volume, 0.0f, 1.0f);
		static_cast<void>(this->group_volume(group));

		(*this->group_volumes_)[static_cast<size_t>(group.index())] = volume;

		// By name, which is a map search per bank: this runs when a setting
		// changes, never per frame, and a slot emptied by adding a null bank
		// is skipped rather than thrown on.
		for (const std::string& bank : this->bank_order_)
		{
			if (this->sound_banks_.contains(bank))
			{
				this->sound_banks_.get(bank)->apply_group_volume(group);
			}
		}
	}

	float AudioResources::group_volume(SoundGroupHandle group) const
	{
		if (!group.valid() ||
			static_cast<size_t>(group.index()) >= this->group_volumes_->size())
		{
			throw std::out_of_range(
				"A sound group was reached through an unresolved handle.");
		}
		return (*this->group_volumes_)[static_cast<size_t>(group.index())];
	}
}

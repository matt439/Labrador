#include "engine/audio/audio_resources.h"

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
	this->sound_banks_.add(sound_bank_name, std::move(sound_bank));
}


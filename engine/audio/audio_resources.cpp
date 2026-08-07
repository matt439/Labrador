#include "engine/audio/audio_resources.h"

AudioResources::SoundBankHandle AudioResources::resolve_sound_bank(
	const std::string& sound_bank_name) const
{
	return this->_sound_banks.resolve(sound_bank_name);
}

SoundBank* AudioResources::get_sound_bank(SoundBankHandle sound_bank) const
{
	return this->_sound_banks.get(sound_bank);
}

SoundBank* AudioResources::get_sound_bank(
	const std::string& sound_bank_name) const
{
	return this->_sound_banks.get(sound_bank_name);
}

void AudioResources::add_sound_bank(const std::string& sound_bank_name,
	std::unique_ptr<SoundBank> sound_bank)
{
	this->_sound_banks.add(sound_bank_name, std::move(sound_bank));
}


#include "engine/assets/resource_manager.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// Every getter here is a one-line forward to a Registry. The three-part
// contract they all used to spell out by hand - the key must exist, the stored
// resource must not have been released by a reset_all_* call, and a failure
// must name what was missing - now lives once, in Registry::get. std::map's
// operator[] provided none of it: it silently inserted a null and returned it,
// and the catch blocks meant to report the failure could never fire because
// operator[] does not throw.

ID3D11ShaderResourceView* ResourceManager::get_texture(
	const std::string& texture_name) const
{
	return this->_textures.get(texture_name);
}

void ResourceManager::add_texture(const std::string& texture_name,
	ID3D11ShaderResourceView* texture)
{
	this->_textures.add(texture_name, texture);
}

SpriteFont* ResourceManager::get_sprite_font(const std::string& font_name) const
{
	return this->_sprite_fonts.get(font_name);
}

void ResourceManager::add_sprite_font(const std::string& font_name,
	std::unique_ptr<SpriteFont> font)
{
	this->_sprite_fonts.add(font_name, std::move(font));
}

SpriteSheet* ResourceManager::get_sprite_sheet(
	const std::string& sprite_sheet_name) const
{
	return this->_sprite_sheets.get(sprite_sheet_name);
}

void ResourceManager::add_sprite_sheet(const std::string& sprite_sheet_name,
	std::unique_ptr<SpriteSheet> sprite_sheet)
{
	this->_sprite_sheets.add(sprite_sheet_name, std::move(sprite_sheet));
}

void ResourceManager::add_sound_bank(const std::string& sound_bank_name,
	std::unique_ptr<SoundBank> sound_bank)
{
	this->_sound_banks.add(sound_bank_name, std::move(sound_bank));
}

SoundBank* ResourceManager::get_sound_bank(
	const std::string& sound_bank_name) const
{
	return this->_sound_banks.get(sound_bank_name);
}

void ResourceManager::reset_all_sprite_fonts()
{
	this->_sprite_fonts.clear();
}

void ResourceManager::reset_all_textures()
{
	this->_textures.clear();
}

void ResourceManager::reset_all_sounds()
{
	for (auto& sound_bank : this->_sound_banks)
	{
		if (sound_bank.second != nullptr)
		{
			sound_bank.second->reset_all_instances();
		}
	}
	this->_sound_banks.clear();
}

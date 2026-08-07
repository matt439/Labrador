#include "engine/assets/resource_loader.h"
#include "engine/assets/sound_bank_loader.h"
#include "engine/assets/sprite_sheet_loader.h"
#include "engine/core/throw_if_failed.h"
#include <DDSTextureLoader.h>
#include <stdexcept>
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

ResourceLoader::ResourceLoader(RenderResources* render_resources,
	AudioResources* audio_resources, ID3D11Device1* device,
	DirectX::AudioEngine* audio_engine) :
	_render_resources(render_resources),
	_audio_resources(audio_resources),
	_device(device),
	_audio_engine(audio_engine)
{
	this->register_builtin_kinds();
}

void ResourceLoader::set_device(ID3D11Device1* device)
{
	this->_device = device;
}

void ResourceLoader::register_kind(const std::string& kind,
	AssetKind asset_kind)
{
	this->_kinds[kind] = std::move(asset_kind);
}

// The four kinds the engine can build without being told anything about the
// game. Each is a pair: how to load it, and what a device restore does to it.
void ResourceLoader::register_builtin_kinds()
{
	const auto texture = [this](const std::string& directory,
		const std::string& name) { this->load_texture(directory, name); };

	const auto font = [this](const std::string& directory,
		const std::string& name) { this->load_sprite_font(directory, name); };

	// A texture and a font are remade outright, so the reload is the load.
	this->register_kind("texture", { texture, texture });
	this->register_kind("font", { font, font });

	this->register_kind("sprite_sheet",
		{
			[this](const std::string& directory, const std::string& name)
			{
				this->load_sprite_sheet(directory, name);
			},
			[this](const std::string& directory, const std::string& name)
			{
				this->reload_sprite_sheet_texture(directory, name);
			}
		});

	// A sound bank is not a device resource, and every live object holds a
	// borrowed SoundBank*, so a device restore must leave it alone.
	this->register_kind("sound_bank",
		{
			[this](const std::string& directory, const std::string& name)
			{
				this->load_sound_bank(directory, name);
			},
			nullptr
		});
}

void ResourceLoader::load_manifest(AssetManifest manifest)
{
	for (const AssetEntry& entry : manifest.entries)
	{
		const auto kind = this->_kinds.find(entry.kind);
		if (kind == this->_kinds.end())
		{
			throw std::out_of_range("Asset '" + entry.name + "' in '" +
				manifest.source_path + "' is of kind '" + entry.kind +
				"', which nothing registered.");
		}
		kind->second.load(entry.directory, entry.name);
	}

	// Kept only once the walk has finished, so the loader never holds a
	// manifest it did not load - a restore replaying a half-loaded one would
	// reload assets that were never there.
	this->_manifest = std::move(manifest);
}

void ResourceLoader::reload_device_resources() const
{
	// The manifest was walked once already, so every kind in it resolves -
	// load_manifest threw if one did not.
	for (const AssetEntry& entry : this->_manifest.entries)
	{
		const AssetKind& kind = this->_kinds.at(entry.kind);
		if (kind.reload_device)
		{
			kind.reload_device(entry.directory, entry.name);
		}
	}
}

void ResourceLoader::load_texture(const std::string& directory,
	const std::string& name) const
{
	const std::string texture_path = directory + name + ".dds";

	ComPtr<ID3D11ShaderResourceView> texture_view;
	ComPtr<ID3D11Resource> resource;
	DX::ThrowIfFailed(
		CreateDDSTextureFromFile(this->_device,
			std::wstring(texture_path.begin(), texture_path.end()).c_str(),
			resource.GetAddressOf(),
			texture_view.ReleaseAndGetAddressOf()));

	this->_render_resources->add_texture(name, texture_view.Get());
}

void ResourceLoader::load_sprite_font(const std::string& directory,
	const std::string& name) const
{
	const std::string font_path = directory + name + ".spritefont";

	try
	{
		this->_render_resources->add_sprite_font(name,
			std::make_unique<SpriteFont>(this->_device,
				std::wstring(font_path.begin(), font_path.end()).c_str()));
	}
	catch (const std::exception&)
	{
		// DirectXTK's what() does not say which font or where - T6 does.
		throw std::out_of_range(
			"SpriteFont " + name + " not found at " + font_path + ".");
	}
}

void ResourceLoader::load_sprite_sheet(const std::string& directory,
	const std::string& name) const
{
	// The sheet's texture is loaded under the sheet's own name: they are one
	// asset with two files, and the manifest names it once.
	this->load_texture(directory, name);

	this->_render_resources->add_sprite_sheet(name,
		sprite_sheet_loader::load((directory + name + ".json").c_str(),
			this->_render_resources->get_texture(name)));
}

void ResourceLoader::reload_sprite_sheet_texture(const std::string& directory,
	const std::string& name) const
{
	this->load_texture(directory, name);

	this->_render_resources->get_sprite_sheet(name)->set_texture(
		this->_render_resources->get_texture(name));
}

void ResourceLoader::load_sound_bank(const std::string& directory,
	const std::string& name) const
{
	const std::string wave_bank_path = directory + name + ".xwb";

	std::unique_ptr<WaveBank> wave_bank;
	try
	{
		wave_bank = std::make_unique<WaveBank>(this->_audio_engine,
			std::wstring(wave_bank_path.begin(),
				wave_bank_path.end()).c_str());
	}
	catch (const std::exception&)
	{
		// DirectXTK's what() is just "WaveBank" - T6 wants the path.
		throw std::runtime_error(
			"Failed to load wave bank: " + wave_bank_path);
	}

	this->_audio_resources->add_sound_bank(name,
		sound_bank_loader::load((directory + name + ".json").c_str(),
			std::move(wave_bank)));
}

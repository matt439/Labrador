#include "engine/assets/resource_loader.h"

#include "engine/assets/sound_bank_loader.h"
#include "engine/assets/sprite_sheet_loader.h"
#include "engine/render/resource_factory.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

using namespace DirectX;

namespace artattack
{
	ResourceLoader::ResourceLoader(RenderResources* render_resources,
		const Renderer* renderer, AudioResources* audio_resources,
		DirectX::AudioEngine* audio_engine) :
		render_resources_(render_resources),
		renderer_(renderer),
		audio_resources_(audio_resources),
		audio_engine_(audio_engine)
	{
		this->register_builtin_kinds();
	}

	void ResourceLoader::register_kind(const std::string& kind,
		AssetKind asset_kind)
	{
		this->kinds_[kind] = std::move(asset_kind);
	}

	// The four kinds the engine can build without being told anything about the
	// game. Each is a pair: how to load it, and what a device restore does to it.
	void ResourceLoader::register_builtin_kinds()
	{
		const auto texture = [this](const std::string& directory,
			const std::string& name, bool /*optional*/)
			{ this->load_texture(directory, name); };

		const auto font = [this](const std::string& directory,
			const std::string& name, bool /*optional*/)
			{ this->load_sprite_font(directory, name); };

		// A texture and a font are remade outright, so the reload is the load.
		this->register_kind("texture", { texture, texture });
		this->register_kind("font", { font, font });

		// A sheet's reload is its texture's reload and nothing else. It used to
		// need a third function that re-seated the new ID3D11ShaderResourceView*
		// into the existing SpriteSheet by hand; the sheet holds a TextureHandle
		// now, the reload refills that handle's slot, and the sheet is untouched.
		this->register_kind("sprite_sheet",
			{
				[this](const std::string& directory, const std::string& name,
					bool /*optional*/)
				{
					this->load_sprite_sheet(directory, name);
				},
				texture
			});

		// A sound bank is not a device resource, and every live object holds a
		// borrowed SoundBank*, so a device restore must leave it alone.
		this->register_kind("sound_bank",
			{
				[this](const std::string& directory, const std::string& name,
					bool optional)
				{
					this->load_sound_bank(optional, directory, name);
				},
				nullptr
			});
	}

	void ResourceLoader::load_manifest(AssetManifest manifest)
	{
		for (const AssetEntry& entry : manifest.entries)
		{
			const auto kind = this->kinds_.find(entry.kind);
			if (kind == this->kinds_.end())
			{
				throw std::out_of_range("Asset '" + entry.name + "' in '" +
					manifest.source_path + "' is of kind '" + entry.kind +
					"', which nothing registered.");
			}
			kind->second.load(entry.directory, entry.name, entry.optional);
		}

		// Kept only once the walk has finished, so the loader never holds a
		// manifest it did not load - a restore replaying a half-loaded one would
		// reload assets that were never there.
		this->manifest_ = std::move(manifest);
	}

	void ResourceLoader::reload_device_resources() const
	{
		// The manifest was walked once already, so every kind in it resolves -
		// load_manifest threw if one did not.
		for (const AssetEntry& entry : this->manifest_.entries)
		{
			const AssetKind& kind = this->kinds_.at(entry.kind);
			if (kind.reload_device)
			{
				kind.reload_device(entry.directory, entry.name, entry.optional);
			}
		}
	}

	// The two kinds that only exist on a device are built by the backend's own
	// factory (engine/render/resource_factory.h). What is left here is the half
	// that decides a manifest entry becomes one - which is this module's job and
	// names no graphics type.
	void ResourceLoader::load_texture(const std::string& directory,
		const std::string& name) const
	{
		load_texture_asset(*this->renderer_, *this->render_resources_,
			directory, name);
	}

	void ResourceLoader::load_sprite_font(const std::string& directory,
		const std::string& name) const
	{
		load_font_asset(*this->renderer_, *this->render_resources_,
			directory, name);
	}

	void ResourceLoader::load_sprite_sheet(const std::string& directory,
		const std::string& name) const
	{
		// The sheet's texture is loaded under the sheet's own name: they are one
		// asset with two files, and the manifest names it once.
		this->load_texture(directory, name);

		this->render_resources_->add_sprite_sheet(name,
			read_sprite_sheet((directory + name + ".json").c_str(),
				this->render_resources_->resolve_texture(name)));
	}

	void ResourceLoader::load_sound_bank(bool optional,
		const std::string& directory, const std::string& name) const
	{
		const std::string wave_bank_path = directory + name + ".xwb";

		std::unique_ptr<WaveBank> wave_bank;
		try
		{
			wave_bank = std::make_unique<WaveBank>(this->audio_engine_,
				std::wstring(wave_bank_path.begin(),
					wave_bank_path.end()).c_str());
		}
		catch (const std::exception&)
		{
			// A bank the manifest marked optional is allowed not to be there,
			// and this is the one substitution the engine makes for a missing
			// file: a bank that resolves everything and plays nothing, so the
			// game runs in silence instead of dying on a file it was never
			// going to have (SoundBank::silent).
			//
			// It is reported rather than swallowed. T6's rule is that a broken
			// contract stops the game with the reason on screen and never
			// aborts silently; a stated-optional asset is not a broken
			// contract, but a person wondering where the sound went should be
			// able to find out without reading this file.
			if (optional)
			{
				std::fprintf(stderr,
					"no audio: '%s' is not there, and the manifest marks this "
					"bank optional, so it plays nothing. See the repository's "
					"README, 'Audio'.\n", wave_bank_path.c_str());

				this->audio_resources_->add_sound_bank(name,
					SoundBank::silent());
				return;
			}

			// DirectXTK's what() is just "WaveBank" - T6 wants the path.
			throw std::runtime_error(
				"Failed to load wave bank: " + wave_bank_path);
		}

		this->audio_resources_->add_sound_bank(name,
			read_sound_bank((directory + name + ".json").c_str(),
				std::move(wave_bank)));
	}
}

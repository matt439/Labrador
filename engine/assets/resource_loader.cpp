#include "engine/assets/resource_loader.h"

#include "engine/assets/sound_bank_loader.h"
#include "engine/assets/sprite_sheet_loader.h"
#include "engine/render/resource_factory.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace labrador
{
	ResourceLoader::ResourceLoader(RenderResources* render_resources,
		const Renderer* renderer, AudioResources* audio_resources,
		AudioDevice* audio_device) :
		render_resources_(render_resources),
		renderer_(renderer),
		audio_resources_(audio_resources),
		audio_device_(audio_device)
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

		// A sheet's reload is its texture's reload and nothing else. No third
		// function re-seating a new device resource into the existing SpriteSheet
		// by hand: the sheet holds a TextureHandle, the reload refills that
		// handle's slot, and the sheet is untouched.
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
		// THE DEFINITION IS READ FIRST, AND THE ORDER IS LOAD-BEARING. A backend
		// with no container answers wave_index out of the wave list this parse
		// produces, so the list has to exist before open_wave_bank is called.
		// sound_bank_loader.h states what it costs: a bank whose container is
		// missing and whose definition is also broken reports the broken
		// definition rather than the missing file. The definition is in every
		// clone and the container is not, so that is the better of the two
		// answers (T6).
		const SoundBankDefinition definition =
			read_sound_bank_definition((directory + name + ".json").c_str());

		AudioDevice::WaveBankHandle wave_bank;
		try
		{
			wave_bank = this->audio_device_->open_wave_bank(directory, name,
				definition.waves);
		}
		catch (const std::runtime_error& missing)
		{
			// std::runtime_error AND NOT std::exception, WHICH IS THE WHOLE
			// DISTINCTION. The seam throws a runtime_error when the container
			// is not there and a std::out_of_range - a logic_error - when it is
			// there and does not hold a wave the definition names. The first is
			// a file a clone was never going to have; the second is a content
			// bug, and catching both here would turn it into silence.
			if (!optional)
			{
				throw;
			}

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
			std::fprintf(stderr,
				"no audio: %s. The manifest marks this bank optional, so it "
				"plays nothing. See the repository's README, 'Audio'.\n",
				missing.what());

			this->audio_resources_->add_sound_bank(name, SoundBank::silent());
			return;
		}

		this->audio_resources_->add_sound_bank(name,
			build_sound_bank(this->audio_device_, wave_bank, definition));
	}
}

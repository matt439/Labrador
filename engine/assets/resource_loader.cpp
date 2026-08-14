#include "engine/assets/resource_loader.h"
#include "engine/assets/sound_bank_loader.h"

#include <cstdio>
#include "engine/assets/sprite_sheet_loader.h"
// The deliberate include renderer.h describes: this is the file that creates
// textures and fonts on a device, so it is the one place outside
// engine/render/<backend>/ that is allowed to name one.
#include "engine/render/d3d11/backend.h"
#include <DDSTextureLoader.h>
#include <wrl/client.h>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
	// The stand-in glyph a font draws when asked for one it does not have.
	//
	// WITHOUT ONE, A MISSING GLYPH IS AN EXCEPTION. DirectXTK's FindGlyph
	// throws when the default character is U+0000, and U+0000 is what every
	// .spritefont in this tree carries, because it is what MakeSpriteFont
	// writes when nobody chooses - along with exactly 95 glyphs, U+0020 to
	// U+007E. Nothing chose that region and nothing records it, so the
	// out-of-the-box behaviour of the engine's own font kind was: any
	// character a text editor inserts on its own - a curly apostrophe, an
	// en-dash - kills the process on the frame that first drew it, which is
	// typically several screens from wherever the string was read.
	//
	// The engine already documented the opposite. text_encoding.h promises
	// that invalid UTF-8 becomes U+FFFD so that "text about to be drawn should
	// show mojibake, not vanish" - and U+FFFD is outside 32..126, so the
	// documented graceful path led directly into the throw it was written to
	// avoid. It leads to a question mark now.
	//
	// Degradation rather than a throw is also what the audio half of this file
	// already does: a missing optional wave bank becomes a bank that plays
	// nothing rather than a dead process.
	//
	// '?' FIRST, ' ' AFTER IT, and nothing if the font has neither. A question
	// mark says "something was here and could not be drawn", which is the
	// mojibake text_encoding.h asks for; a space says only that the text is
	// oddly spaced, and is the better of the two only when there is no
	// question mark to be had. A font with neither is not a text font - an
	// icon atlas, say - and is left exactly as it was rather than warned
	// about, because there is nothing wrong with it.
	//
	// Asked before it is set, never caught: SetDefaultCharacter throws for a
	// character the font does not have, and that is the throw this whole
	// function exists to stop happening.
	void install_default_character(DirectX::SpriteFont& font)
	{
		if (font.ContainsCharacter(L'?'))
		{
			font.SetDefaultCharacter(L'?');
			return;
		}
		if (font.ContainsCharacter(L' '))
		{
			font.SetDefaultCharacter(L' ');
		}
	}
}

namespace artattack
{
	ResourceLoader::ResourceLoader(RenderResources* render_resources,
		AudioResources* audio_resources, ID3D11Device1* device,
		DirectX::AudioEngine* audio_engine) :
		render_resources_(render_resources),
		audio_resources_(audio_resources),
		device_(device),
		audio_engine_(audio_engine)
	{
		this->register_builtin_kinds();
	}

	void ResourceLoader::set_device(ID3D11Device1* device)
	{
		this->device_ = device;
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

	void ResourceLoader::load_texture(const std::string& directory,
		const std::string& name) const
	{
		const std::string texture_path = directory + name + ".dds";

		ComPtr<ID3D11ShaderResourceView> texture_view;
		ComPtr<ID3D11Resource> resource;
		const HRESULT result = CreateDDSTextureFromFile(this->device_,
			std::wstring(texture_path.begin(), texture_path.end()).c_str(),
			resource.GetAddressOf(),
			texture_view.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			// ThrowIfFailed's message is the HRESULT and nothing else, so the
			// commonest failure here - a texture named in the manifest that is
			// not on disk - arrived as eight hex digits. The font loader twenty
			// lines below has always named its file; this now does too (T6).
			char hresult[16] = {};
			std::snprintf(hresult, sizeof(hresult), "0x%08X",
				static_cast<unsigned int>(result));
			throw std::runtime_error("Failed to load texture: " + texture_path +
				" (" + hresult + ")");
		}

		this->render_resources_->impl()->add_texture(name, texture_view.Get());
	}

	void ResourceLoader::load_sprite_font(const std::string& directory,
		const std::string& name) const
	{
		const std::string font_path = directory + name + ".spritefont";

		std::unique_ptr<SpriteFont> font;
		try
		{
			font = std::make_unique<SpriteFont>(this->device_,
				std::wstring(font_path.begin(), font_path.end()).c_str());
		}
		catch (const std::exception&)
		{
			// DirectXTK's what() does not say which font or where - T6 does.
			throw std::out_of_range(
				"SpriteFont " + name + " not found at " + font_path + ".");
		}

		install_default_character(*font);
		this->render_resources_->impl()->add_sprite_font(name, std::move(font));
	}

	void ResourceLoader::load_sprite_sheet(const std::string& directory,
		const std::string& name) const
	{
		// The sheet's texture is loaded under the sheet's own name: they are one
		// asset with two files, and the manifest names it once.
		this->load_texture(directory, name);

		this->render_resources_->impl()->add_sprite_sheet(name,
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

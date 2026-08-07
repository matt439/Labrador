#pragma once

#include "engine/assets/asset_manifest.h"
#include "engine/audio/audio_resources.h"
#include "engine/render/render_resources.h"
#include <Audio.h>
#include <d3d11_1.h>
#include <functional>
#include <map>
#include <string>

// Turns a manifest into loaded resources, and does it again when the GPU
// throws the first set away.
//
// It knows how to build four things - a texture, a font, a sprite sheet, a
// sound bank - and nothing whatever about which ones a given game has: the
// names are the manifest's business (T7). A game needing a fifth kind teaches
// the loader one, rather than the engine learning that game's vocabulary (T1).
class ResourceLoader
{
public:
	// Builds one asset from a manifest entry. A kind owns its own file naming,
	// extension included, which is why this is handed the directory and the
	// name rather than a finished path.
	using LoadAsset = std::function<void(const std::string& directory,
		const std::string& name)>;

	// A kind of asset, and what a device loss does to it. `reload_device` runs
	// in place of `load` on a device restore; leave it empty for a kind the GPU
	// does not hold - a sound bank, a level definition - and the restore skips
	// it, which is what keeps borrowed pointers into those alive.
	//
	// It is a second function rather than a flag because rebuilding is not
	// always re-loading: a font is remade outright, while a sprite sheet only
	// re-seats its texture and keeps the frame and strip tables that every
	// handle resolved against it indexes into.
	struct AssetKind
	{
		LoadAsset load;
		LoadAsset reload_device;
	};

	// The stores to fill are handed in - the loader fills stores, it does not
	// own them.
	ResourceLoader(RenderResources* render_resources,
		AudioResources* audio_resources, ID3D11Device1* device,
		DirectX::AudioEngine* audio_engine);

	// The built-in kinds hold `this`, so a copy would quietly load into the
	// original's stores.
	ResourceLoader(const ResourceLoader&) = delete;
	ResourceLoader& operator=(const ResourceLoader&) = delete;

	// Teaches the loader a kind of asset a manifest may name. The engine
	// registers "texture", "font", "sprite_sheet" and "sound_bank" at
	// construction; anything else is the game's to register, before it loads a
	// manifest that names one. Registering a kind twice replaces it.
	void register_kind(const std::string& kind, AssetKind asset_kind);

	// Loads every asset the manifest names, in the order it names them, and
	// keeps the manifest so that a device restore can replay it. A second
	// manifest replaces the first as the thing a restore replays, so a game
	// with more than one loads them as one.
	//
	// Throws std::out_of_range naming the kind, the asset and the manifest file
	// if an entry names a kind nobody registered - which is a content bug, and
	// this is the line that should report it (T6). Whatever loaded before the
	// bad entry stays loaded; the manifest is not kept.
	void load_manifest(AssetManifest manifest);

	// Rebuilds what lives on the GPU after a device loss, by walking the same
	// manifest again. Replaying the same names is the load-bearing part:
	// drawables hold handles to registry slots and a slot belongs to a name, so
	// a rebuild producing the same *count* under different names would leave
	// every handle reading the wrong resource. Walking the manifest makes that
	// true by construction, where a second list kept in step by hand only makes
	// it true today.
	//
	// Kinds with no `reload_device` are skipped: they are not device resources,
	// and callers hold borrowed SoundBank* and level-definition pointers into
	// them that a reload would invalidate.
	void reload_device_resources() const;

	// The device changes identity on restore, so the loader has to be told.
	void set_device(ID3D11Device1* device);

private:
	RenderResources* _render_resources = nullptr;
	AudioResources* _audio_resources = nullptr;
	ID3D11Device1* _device = nullptr;
	DirectX::AudioEngine* _audio_engine = nullptr;

	std::map<std::string, AssetKind> _kinds;

	// What was loaded, kept for the replay above. Empty until load_manifest,
	// so a device restore before the first load rebuilds nothing.
	AssetManifest _manifest;

	void register_builtin_kinds();

	void load_texture(const std::string& directory,
		const std::string& name) const;
	void load_sprite_font(const std::string& directory,
		const std::string& name) const;
	void load_sprite_sheet(const std::string& directory,
		const std::string& name) const;
	void load_sound_bank(const std::string& directory,
		const std::string& name) const;

	// Reloads the sheet's .dds and points the existing SpriteSheet at it. The
	// frame and animation-strip tables are device-independent, so they are left
	// alone and every cached SpriteSheet*/AnimationStrip* stays valid.
	void reload_sprite_sheet_texture(const std::string& directory,
		const std::string& name) const;
};

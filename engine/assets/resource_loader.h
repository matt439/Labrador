#pragma once

#include "engine/assets/asset_manifest.h"
#include "engine/audio/audio_device.h"
#include "engine/audio/audio_resources.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include <functional>
#include <map>
#include <string>

namespace labrador
{
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
		// `optional` is the manifest's own word for "the game still runs
		// without this" (asset_manifest.h). A kind that has no substitute to
		// offer ignores it and throws as it always did; the sound bank is the
		// one that has one.
		using LoadAsset = std::function<void(const std::string& directory,
			const std::string& name, bool optional)>;

		// A kind of asset, and what a device loss does to it. `reload_device` runs
		// in place of `load` on a device restore; leave it empty for a kind the GPU
		// does not hold - a sound bank, a level definition - and the restore skips
		// it, which is what keeps borrowed pointers into those alive.
		//
		// It is a second function rather than a flag because rebuilding is not
		// always re-loading: a sprite sheet's device half is its texture, and
		// the frame and strip tables that every handle indexes into are not
		// rebuilt at all.
		//
		// THE CRITERION IS WHAT THE GPU HOLDS, NEVER WHETHER THIS BUILD'S
		// BACKEND CAN LOSE A DEVICE, and this paragraph did not say so until a
		// port asked where the rebuild belonged. Two of the five backends never
		// call reload_device at all - a WGL context is not lost, and the null
		// one has nothing to lose - so a game written and run against either of
		// those presets can leave every one of these empty and watch nothing go
		// wrong. It is the same source a Direct3D build compiles:
		// LABRADOR_RENDER_BACKEND picks the backend at configure time (T5), so
		// what varies is which build ever runs this function, not which source
		// has to write it. engine/render/SEAM.md carries the argument in its
		// section 8, and
		// tests/assets/resource_loader_tests.cpp pins what a restore does with
		// what is written here.
		struct AssetKind
		{
			LoadAsset load;
			LoadAsset reload_device;
		};

		// The stores to fill are handed in - the loader fills stores, it does not
		// own them.
		//
		// The renderer rather than a device, and this file names no graphics type
		// as a result: the two kinds that need one go through
		// engine/render/resource_factory.h, which reads the device off the
		// renderer at the moment it builds. That also deletes a caller
		// obligation. A device restore hands back a different device, so a loader
		// holding one had to be re-seated from on_device_restored - a rule with
		// no home, stated in two places, enforced in none.
		// The audio device rather than a library's engine object, and for the
		// same reason: this file named DirectX::AudioEngine in a constructor
		// parameter and included <Audio.h> to do it, which put XAudio2's
		// headers on the command line of every client that loads an asset.
		// engine/audio/audio_device.h is the seam now, and which audio API is
		// behind it is chosen in CMake and named nowhere above it.
		ResourceLoader(RenderResources* render_resources,
			const Renderer* renderer, AudioResources* audio_resources,
			AudioDevice* audio_device);

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

	private:
		RenderResources* render_resources_ = nullptr;
		const Renderer* renderer_ = nullptr;
		AudioResources* audio_resources_ = nullptr;
		AudioDevice* audio_device_ = nullptr;

		std::map<std::string, AssetKind> kinds_;

		// What was loaded, kept for the replay above. Empty until load_manifest,
		// so a device restore before the first load rebuilds nothing.
		AssetManifest manifest_;

		void register_builtin_kinds();

		void load_texture(const std::string& directory,
			const std::string& name) const;
		void load_sprite_font(const std::string& directory,
			const std::string& name) const;
		void load_sprite_sheet(const std::string& directory,
			const std::string& name) const;
		void load_sound_bank(bool optional, const std::string& directory,
			const std::string& name) const;
	};
}

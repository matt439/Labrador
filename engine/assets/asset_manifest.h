#pragma once

#include <string>
#include <vector>

namespace labrador
{
	// One asset to load: what it is, where it lives, and what it is called.
	//
	// `kind` is a key into whatever kinds the loader has been taught, not
	// something this type interprets. The engine ships four and a game adds its
	// own, so the set is open by construction and no engine header has to learn
	// the word "level" (T1).
	//
	// `name` is both the file's stem and the registry name the asset lands under.
	// They are deliberately one string: a manifest saying "sprite_sheet_1" names a
	// file a person can go and find, where a loader told both separately would let
	// the two drift.
	struct AssetEntry
	{
		std::string kind;
		std::string directory;
		std::string name;

		// Whether the game still runs when this file is not there.
		//
		// False for everything by default, because T6 says a broken contract
		// stops the game dead with the reason on screen. This is how a piece
		// of content states that its absence is not a broken contract - and
		// it is stated in the manifest, by the person who knows, rather than
		// guessed at by a loader.
		//
		// The one case that needs it: the paint-shooter's wave bank is built
		// from source audio that cannot be distributed (see the repository's
		// README), so a fresh clone has no ./sounds/sound_bank_1.xwb and used
		// to throw at startup on the very file the shipped manifest named.
		// What the loader substitutes is the kind's business; what this says
		// is only that it may.
		bool optional = false;
	};

	// Everything a game is made of, as data the loader walks rather than filenames
	// spelt into source (T7). Adding a level or a font is editing this file; the
	// loader is not rebuilt and does not change.
	//
	// It is also what a device restore replays, which is why the loader keeps it
	// rather than consuming it - see ResourceLoader::reload_device_resources.
	struct AssetManifest
	{
		// Where it was read from, so a bad entry can name the file it came out of
		// and not just itself (T6).
		std::string source_path;

		// Walked in order: the file's order is the load order, so a kind that has
		// to exist before another can be sequenced by moving a line.
		std::vector<AssetEntry> entries;
	};
}

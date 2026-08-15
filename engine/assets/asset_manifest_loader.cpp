#include "engine/assets/asset_manifest_loader.h"
#include "engine/assets/json.h"

namespace labrador
{
	AssetManifest read_asset_manifest(const char* json_path)
	{
		// Every read below is a checked one. The manifest is the one file a
		// person edits by hand to add an asset, so a typo comes back as a
		// sentence naming the file and the group it was in rather than as an
		// assert a Release build has disarmed (T6) - which is what JsonValue is
		// for, and this loader is no longer the only thing that has it.
		const JsonDocument document = read_json_file(json_path);

		AssetManifest manifest;
		manifest.source_path = document.source_path();

		const JsonValue groups = document.root().array("assets");
		for (size_t group_index = 0; group_index < groups.size(); ++group_index)
		{
			const JsonValue group = groups.at(group_index);

			// The kind and directory are the group's; only the name varies down
			// the list, which is the whole reason the file groups and the loader
			// does not.
			AssetEntry entry;
			entry.kind = group.string("kind");
			entry.directory = group.string("directory");

			// Absent means required, which is the safe default and the one
			// every existing manifest already meant.
			entry.optional =
				group.has("optional") ? group.boolean("optional") : false;

			const JsonValue names = group.array("names");
			for (size_t name_index = 0; name_index < names.size(); ++name_index)
			{
				entry.name = names.at(name_index).as_string();
				manifest.entries.push_back(entry);
			}
		}

		return manifest;
	}
}

#include "engine/assets/asset_manifest_loader.h"
#include "engine/assets/json_loader.h"
#include <stdexcept>
#include <string>

using namespace rapidjson;

namespace artattack
{
	namespace
	{
		// rapidjson's operator[] asserts on a missing member and GetArray() asserts
		// on the wrong type - a crash in debug and undefined behaviour in release.
		// The manifest is the one file a person edits by hand to add an asset, so
		// every read below goes through these instead and a typo comes back as a
		// sentence naming the file and the group it was in (T6).
		[[noreturn]] void fail(const std::string& path, const std::string& what)
		{
			throw std::runtime_error(
				"read_asset_manifest - '" + path + "' " + what);
		}

		const Value& require_member(const Value& parent, const char* key,
			const std::string& path, const std::string& context)
		{
			const auto member = parent.FindMember(key);
			if (member == parent.MemberEnd())
			{
				fail(path, context + " has no '" + key + "'");
			}
			return member->value;
		}

		const Value& require_array(const Value& parent, const char* key,
			const std::string& path, const std::string& context)
		{
			const Value& value = require_member(parent, key, path, context);
			if (!value.IsArray())
			{
				fail(path, context + ": '" + key + "' is not an array");
			}
			return value;
		}

		std::string require_string(const Value& parent, const char* key,
			const std::string& path, const std::string& context)
		{
			const Value& value = require_member(parent, key, path, context);
			if (!value.IsString())
			{
				fail(path, context + ": '" + key + "' is not a string");
			}
			return value.GetString();
		}
	}

	AssetManifest read_asset_manifest(const char* json_path)
	{
		const Document doc = read_json_file(json_path);

		AssetManifest manifest;
		manifest.source_path = json_path;
		const std::string& path = manifest.source_path;

		if (!doc.IsObject())
		{
			fail(path, "is not a JSON object");
		}

		const Value& groups = require_array(doc, "assets", path, "the manifest");

		int group_index = 0;
		for (const Value& group : groups.GetArray())
		{
			const std::string context =
				"assets[" + std::to_string(group_index++) + "]";

			if (!group.IsObject())
			{
				fail(path, context + " is not an object");
			}

			// The kind and directory are the group's; only the name varies down the
			// list, which is the whole reason the file groups and the loader does
			// not.
			AssetEntry entry;
			entry.kind = require_string(group, "kind", path, context);
			entry.directory = require_string(group, "directory", path, context);

			for (const Value& name : require_array(group, "names", path,
				context).GetArray())
			{
				if (!name.IsString())
				{
					fail(path, context + ": every entry of 'names' must be a "
						"string");
				}
				entry.name = name.GetString();
				manifest.entries.push_back(entry);
			}
		}

		return manifest;
	}
}

#include <doctest/doctest.h>
#include "engine/assets/resource_loader.h"
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace labrador;

// What a device restore does to a manifest, asserted without a device.
//
// This is the file engine/render/renderer.h's settled note on
// AssetKind::reload_device points at. The question that note closed was where
// the device-loss rebuild belongs - on the loader, on DeviceNotify, or nowhere
// - and the answer is the loader, because a rebuild has to refill the slots
// the old resources were in and the manifest is the only thing that knows
// their names. That answer is worth a test rather than only a paragraph: the
// order, the skipping and the replacement below are what "refills slots rather
// than makes new ones" actually means, and nothing pinned them before.
//
// IT RUNS IN ALL FOUR CONFIGURATIONS, which is the point. Every loader here is
// built from four null pointers, and that is not a shortcut - it is the first
// thing this file pins. ResourceLoader dereferences what it was handed only
// from the four kinds it registers itself, so a game's own kind reaches none of
// them, and the whole of the restore contract is answerable with no device, no
// window and no audio engine.

namespace
{
	// One call to a kind, and which half of the kind made it.
	struct Call
	{
		std::string kind;
		std::string directory;
		std::string name;
		bool optional = false;

		// reload_device rather than load. A sheet's two halves are genuinely
		// different functions (resource_loader.h says why), so a test that
		// could not tell them apart would pass on a loader that ran the wrong
		// one.
		bool restore = false;
	};

	ResourceLoader::LoadAsset recorder(std::vector<Call>* calls,
		std::string kind, bool restore)
	{
		return [calls, kind = std::move(kind), restore](
			const std::string& directory, const std::string& name,
			bool optional)
			{
				calls->push_back(Call{ kind, directory, name, optional,
					restore });
			};
	}

	// A kind the GPU holds: its restore is a second function that says so.
	ResourceLoader::AssetKind device_kind(std::vector<Call>* calls,
		const std::string& kind)
	{
		return ResourceLoader::AssetKind{ recorder(calls, kind, false),
			recorder(calls, kind, true) };
	}

	// A kind the GPU does not hold - a sound bank, a level definition. The
	// empty half is what makes a restore skip it, which is what keeps every
	// borrowed pointer into it alive.
	ResourceLoader::AssetKind engine_kind(std::vector<Call>* calls,
		const std::string& kind)
	{
		return ResourceLoader::AssetKind{ recorder(calls, kind, false),
			nullptr };
	}

	AssetManifest manifest_of(std::vector<AssetEntry> entries)
	{
		AssetManifest manifest;
		manifest.source_path = "resource_loader_tests.json";
		manifest.entries = std::move(entries);
		return manifest;
	}

	AssetEntry entry(const std::string& kind, const std::string& name)
	{
		AssetEntry asset_entry;
		asset_entry.kind = kind;
		asset_entry.directory = "./" + kind + "s/";
		asset_entry.name = name;
		return asset_entry;
	}
}

namespace ResourceLoaderTests
{
	TEST_SUITE("ResourceLoaderTests")
	{
		TEST_CASE("a restore replays only the kinds that hold something on "
			"the device")
		{
			std::vector<Call> calls;
			ResourceLoader loader(nullptr, nullptr, nullptr, nullptr);
			loader.register_kind("texture_like", device_kind(&calls,
				"texture_like"));
			loader.register_kind("bank_like", engine_kind(&calls, "bank_like"));

			loader.load_manifest(manifest_of({
				entry("texture_like", "first"),
				entry("bank_like", "sounds"),
				entry("texture_like", "second") }));

			REQUIRE(calls.size() == 3);
			CHECK(calls[0].name == "first");
			CHECK(calls[1].name == "sounds");
			CHECK(calls[2].name == "second");
			CHECK_FALSE(calls[0].restore);

			calls.clear();
			loader.reload_device_resources();

			// The bank is not there, and the two textures are - in the order
			// the manifest names them, because a kind that has to exist before
			// another is sequenced by moving a line and a restore has to
			// sequence them the same way.
			REQUIRE(calls.size() == 2);
			CHECK(calls[0].kind == "texture_like");
			CHECK(calls[0].name == "first");
			CHECK(calls[0].restore);
			CHECK(calls[1].name == "second");
			CHECK(calls[1].restore);
		}

		TEST_CASE("a restore replays the entry, not the name alone")
		{
			std::vector<Call> calls;
			ResourceLoader loader(nullptr, nullptr, nullptr, nullptr);
			loader.register_kind("texture_like", device_kind(&calls,
				"texture_like"));

			AssetEntry asset_entry = entry("texture_like", "sheet");
			asset_entry.directory = "./deep/nested/";
			asset_entry.optional = true;
			loader.load_manifest(manifest_of({ asset_entry }));
			calls.clear();

			loader.reload_device_resources();

			// A kind owns its own file naming, so the directory is as
			// load-bearing as the name; and `optional` is the manifest's word
			// for what a kind may substitute, which a rebuild needs as much as
			// the first load did.
			REQUIRE(calls.size() == 1);
			CHECK(calls[0].directory == "./deep/nested/");
			CHECK(calls[0].name == "sheet");
			CHECK(calls[0].optional);
		}

		TEST_CASE("a restore before any load rebuilds nothing")
		{
			std::vector<Call> calls;
			ResourceLoader loader(nullptr, nullptr, nullptr, nullptr);
			loader.register_kind("texture_like", device_kind(&calls,
				"texture_like"));

			loader.reload_device_resources();

			CHECK(calls.empty());
		}

		TEST_CASE("a second manifest replaces what a restore replays")
		{
			std::vector<Call> calls;
			ResourceLoader loader(nullptr, nullptr, nullptr, nullptr);
			loader.register_kind("texture_like", device_kind(&calls,
				"texture_like"));

			loader.load_manifest(manifest_of({
				entry("texture_like", "menu") }));
			loader.load_manifest(manifest_of({
				entry("texture_like", "level") }));
			calls.clear();

			loader.reload_device_resources();

			// resource_loader.h states this rather than leaving it to be
			// discovered: a game with more than one manifest loads them as one,
			// because the second is what a restore replays and the first is
			// gone.
			REQUIRE(calls.size() == 1);
			CHECK(calls[0].name == "level");
		}

		TEST_CASE("a manifest that named a kind nobody registered is not "
			"replayed")
		{
			std::vector<Call> calls;
			ResourceLoader loader(nullptr, nullptr, nullptr, nullptr);
			loader.register_kind("texture_like", device_kind(&calls,
				"texture_like"));

			CHECK_THROWS_AS(loader.load_manifest(manifest_of({
				entry("texture_like", "first"),
				entry("level", "one") })), std::out_of_range);

			// Whatever loaded before the bad entry stays loaded - it is on the
			// GPU, and nothing unloads it.
			REQUIRE(calls.size() == 1);
			CHECK(calls[0].name == "first");

			calls.clear();
			loader.reload_device_resources();

			// But the manifest was never kept, so the restore has nothing to
			// walk. A half-loaded manifest replayed in full would rebuild
			// assets that were never there in the first place.
			CHECK(calls.empty());
		}

		TEST_CASE("registering a kind twice replaces both of its halves")
		{
			std::vector<Call> calls;
			ResourceLoader loader(nullptr, nullptr, nullptr, nullptr);
			loader.register_kind("thing", device_kind(&calls, "first_owner"));
			loader.register_kind("thing", device_kind(&calls, "second_owner"));

			loader.load_manifest(manifest_of({ entry("thing", "asset") }));
			loader.reload_device_resources();

			REQUIRE(calls.size() == 2);
			CHECK(calls[0].kind == "second_owner");
			CHECK(calls[1].kind == "second_owner");
			CHECK(calls[1].restore);
		}
	}
}

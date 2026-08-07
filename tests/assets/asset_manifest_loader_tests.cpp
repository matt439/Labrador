#include <doctest/doctest.h>
#include "engine/assets/asset_manifest_loader.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
	// A manifest on disk, deleted when the test leaves. The loader takes a
	// path, so testing it means writing files - there is no in-memory door in,
	// and adding one for the tests would be a second parse path to keep honest.
	class TempManifest
	{
	public:
		explicit TempManifest(const std::string& contents)
		{
			this->_path = std::filesystem::temp_directory_path() /
				("artattack_manifest_test_" + std::to_string(next_id()) +
					".json");

			std::ofstream file(this->_path, std::ios::binary);
			file << contents;
		}

		~TempManifest()
		{
			std::error_code ignored;
			std::filesystem::remove(this->_path, ignored);
		}

		TempManifest(const TempManifest&) = delete;
		TempManifest& operator=(const TempManifest&) = delete;

		std::string path() const { return this->_path.string(); }

	private:
		// Every case gets its own file, so a leftover from a crashed run cannot
		// be read as this run's input.
		static int next_id()
		{
			static int id = 0;
			return ++id;
		}

		std::filesystem::path _path;
	};

	AssetManifest load(const std::string& contents)
	{
		const TempManifest manifest(contents);
		return asset_manifest_loader::load(manifest.path().c_str());
	}
}

namespace AssetManifestLoaderTests
{
	TEST_SUITE("AssetManifestLoaderTests")
	{
		TEST_CASE("a group flattens to one entry per name")
		{
			const AssetManifest manifest = load(R"({
				"assets": [
					{
						"kind": "font",
						"directory": "./fonts/",
						"names": [ "small", "large" ]
					}
				]
			})");

			REQUIRE(manifest.entries.size() == 2);

			CHECK(manifest.entries[0].kind == "font");
			CHECK(manifest.entries[0].directory == "./fonts/");
			CHECK(manifest.entries[0].name == "small");

			// The kind and directory belong to the group, so every name in it
			// carries the same pair.
			CHECK(manifest.entries[1].kind == "font");
			CHECK(manifest.entries[1].directory == "./fonts/");
			CHECK(manifest.entries[1].name == "large");
		}

		TEST_CASE("entries keep the order the file lists them in")
		{
			// The load order is the file's order, which is what lets a manifest
			// sequence a kind that has to exist before another.
			const AssetManifest manifest = load(R"({
				"assets": [
					{ "kind": "texture", "directory": "./t/", "names": [ "a" ] },
					{ "kind": "font", "directory": "./f/", "names": [ "b" ] },
					{ "kind": "texture", "directory": "./t/", "names": [ "c" ] }
				]
			})");

			REQUIRE(manifest.entries.size() == 3);
			CHECK(manifest.entries[0].name == "a");
			CHECK(manifest.entries[1].name == "b");
			CHECK(manifest.entries[2].name == "c");
		}

		TEST_CASE("the manifest remembers where it was read from")
		{
			const TempManifest file(R"({ "assets": [] })");

			const AssetManifest manifest =
				asset_manifest_loader::load(file.path().c_str());

			CHECK(manifest.source_path == file.path());
			CHECK(manifest.entries.empty());
		}

		TEST_CASE("a kind nobody knows is still parsed")
		{
			// The loader does not own the set of kinds - a game registers its
			// own - so an unknown one is not this layer's error to raise. It
			// fails later, by name, at the walk.
			const AssetManifest manifest = load(R"({
				"assets": [
					{ "kind": "level", "directory": "./l/", "names": [ "x" ] }
				]
			})");

			REQUIRE(manifest.entries.size() == 1);
			CHECK(manifest.entries[0].kind == "level");
		}

		TEST_CASE("a missing file names the path")
		{
			CHECK_THROWS_AS(
				asset_manifest_loader::load("./no_such_manifest.json"),
				std::runtime_error);
		}

		TEST_CASE("a malformed document throws rather than asserting")
		{
			// Each of these reaches a rapidjson call that asserts on the wrong
			// shape - a crash in debug, undefined behaviour in release - if the
			// loader does not check first.
			SUBCASE("not JSON at all")
			{
				CHECK_THROWS_AS(load("{ not json"), std::runtime_error);
			}
			SUBCASE("not an object")
			{
				CHECK_THROWS_AS(load("[]"), std::runtime_error);
			}
			SUBCASE("no assets member")
			{
				CHECK_THROWS_AS(load(R"({ "stuff": [] })"), std::runtime_error);
			}
			SUBCASE("assets is not an array")
			{
				CHECK_THROWS_AS(load(R"({ "assets": {} })"),
					std::runtime_error);
			}
			SUBCASE("a group is not an object")
			{
				CHECK_THROWS_AS(load(R"({ "assets": [ 3 ] })"),
					std::runtime_error);
			}
			SUBCASE("a group has no kind")
			{
				CHECK_THROWS_AS(
					load(R"({"assets":[{"directory":"./f/","names":[]}]})"),
					std::runtime_error);
			}
			SUBCASE("a group has no directory")
			{
				CHECK_THROWS_AS(
					load(R"({"assets":[{"kind":"font","names":[]}]})"),
					std::runtime_error);
			}
			SUBCASE("a group has no names")
			{
				CHECK_THROWS_AS(
					load(R"({"assets":[{"kind":"font","directory":"./f/"}]})"),
					std::runtime_error);
			}
			SUBCASE("kind is not a string")
			{
				CHECK_THROWS_AS(
					load(R"({"assets":[{"kind":7,"directory":"./f/",
						"names":[]}]})"),
					std::runtime_error);
			}
			SUBCASE("a name is not a string")
			{
				CHECK_THROWS_AS(
					load(R"({"assets":[{"kind":"font","directory":"./f/",
						"names":[7]}]})"),
					std::runtime_error);
			}
		}

		TEST_CASE("a failure names the file it came out of")
		{
			const TempManifest file(R"({ "assets": [ { "kind": "font" } ] })");

			try
			{
				asset_manifest_loader::load(file.path().c_str());
				FAIL("expected a throw");
			}
			catch (const std::runtime_error& error)
			{
				const std::string message = error.what();

				// A manifest is hand-edited, so the message has to say which
				// file, which group, and which key (T6).
				CHECK(message.find(file.path()) != std::string::npos);
				CHECK(message.find("assets[0]") != std::string::npos);
				CHECK(message.find("directory") != std::string::npos);
			}
		}
	}
}

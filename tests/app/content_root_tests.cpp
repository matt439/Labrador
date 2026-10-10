#include <doctest/doctest.h>

#include "engine/app/content_root.h"
#include "engine/assets/asset_manifest.h"
#include "engine/assets/asset_manifest_loader.h"
#include "engine/core/byte_reader.h"
#include "engine/core/file_path.h"

#include <filesystem>
#include <fstream>
#include <string>

using labrador::AssetManifest;
using labrador::anchored_to_source;
using labrador::executable_directory;
using labrador::resolved_under;

// Where a game's content is, asserted from a process that is somewhere else.
//
// The review's reproduction was the sample's loader run from the repository
// root: "cannot open './manifest.json'" (docs/review/gpt6/README.md, G6-09).
// The cases below never change the working directory - a test that did would
// change it for every case after it - and never need to: the policy is that
// the working directory is not consulted, so the assertions are about what a
// relative path resolves to, and the last case reads a real manifest from a
// folder the process was not started in.

#if defined(__EMSCRIPTEN__)
TEST_CASE("the executable directory is the root content is packed into")
{
	// A browser has no executable, and a browser build packs a game's
	// content into the root of the in-memory file system.
	CHECK(executable_directory() == "/");
}
#else
TEST_CASE("the executable directory is where this test binary is")
{
	const std::string directory = executable_directory();

	REQUIRE(!directory.empty());
	CHECK((directory.back() == '\\' || directory.back() == '/'));
	CHECK(labrador::path_from_utf8(directory).is_absolute());

	// AppTests.exe is the executable, so it is in there.
	CHECK(std::filesystem::exists(
		labrador::path_from_utf8(directory) / "AppTests.exe"));
}
#endif

// The next two cases are spelt in each platform's own paths, because an
// absolute path is a platform's idea: "C:\game" is a relative name with a
// backslash in it to a POSIX file system, and "/game" is relative to the
// current drive on Windows.
#if defined(_WIN32)
TEST_CASE("a relative path resolves under the directory and an absolute one does not")
{
	CHECK(resolved_under("C:\\game\\", "./manifest.json") ==
		"C:\\game\\./manifest.json");
	CHECK(resolved_under("C:\\game\\", "content/manifest.json") ==
		"C:\\game\\content/manifest.json");

	// A directory without its separator gets one, so the two halves cannot
	// run together into a different name.
	CHECK(resolved_under("C:\\game", "manifest.json") ==
		"C:\\game\\manifest.json");

	// Absolute paths are the game's own decision and are not touched.
	CHECK(resolved_under("C:\\game\\", "D:\\content\\manifest.json") ==
		"D:\\content\\manifest.json");

	// An empty directory anchors to nothing, which leaves the path as the
	// working-directory-relative one it was.
	CHECK(resolved_under("", "./fonts/") == "./fonts/");
}

TEST_CASE("a manifest's relative directories are anchored to its own file")
{
	AssetManifest manifest;
	manifest.source_path = "C:\\game\\content\\manifest.json";
	manifest.entries.push_back({ "font", "./fonts/", "a", false });
	manifest.entries.push_back({ "texture", "textures/", "b", false });
	manifest.entries.push_back({ "level", "D:\\levels\\", "c", true });

	const AssetManifest anchored = anchored_to_source(manifest);

	REQUIRE(anchored.entries.size() == 3);
	CHECK(anchored.entries[0].directory == "C:\\game\\content\\./fonts/");
	CHECK(anchored.entries[1].directory == "C:\\game\\content\\textures/");
	CHECK(anchored.entries[2].directory == "D:\\levels\\");

	// Everything that is not a directory rides through untouched, the
	// optional flag included - it is the loader's, not this file's.
	CHECK(anchored.entries[2].optional);
	CHECK(anchored.entries[0].name == "a");
	CHECK(anchored.source_path == manifest.source_path);
}
#else
TEST_CASE("a relative path resolves under the directory and an absolute one does not")
{
	CHECK(resolved_under("/game/", "./manifest.json") ==
		"/game/./manifest.json");
	CHECK(resolved_under("/game/", "content/manifest.json") ==
		"/game/content/manifest.json");

	// A directory without its separator gets one, so the two halves cannot
	// run together into a different name.
	CHECK(resolved_under("/game", "manifest.json") == "/game/manifest.json");

	// Absolute paths are the game's own decision and are not touched.
	CHECK(resolved_under("/game/", "/content/manifest.json") ==
		"/content/manifest.json");

	// An empty directory anchors to nothing, which leaves the path as the
	// working-directory-relative one it was.
	CHECK(resolved_under("", "./fonts/") == "./fonts/");
}

TEST_CASE("a manifest's relative directories are anchored to its own file")
{
	AssetManifest manifest;
	manifest.source_path = "/game/content/manifest.json";
	manifest.entries.push_back({ "font", "./fonts/", "a", false });
	manifest.entries.push_back({ "texture", "textures/", "b", false });
	manifest.entries.push_back({ "level", "/levels/", "c", true });

	const AssetManifest anchored = anchored_to_source(manifest);

	REQUIRE(anchored.entries.size() == 3);
	CHECK(anchored.entries[0].directory == "/game/content/./fonts/");
	CHECK(anchored.entries[1].directory == "/game/content/textures/");
	CHECK(anchored.entries[2].directory == "/levels/");

	CHECK(anchored.entries[2].optional);
	CHECK(anchored.entries[0].name == "a");
	CHECK(anchored.source_path == manifest.source_path);
}
#endif

TEST_CASE("a manifest with no source has nothing to anchor to")
{
	AssetManifest manifest;
	manifest.entries.push_back({ "font", "./fonts/", "a", false });

	const AssetManifest anchored = anchored_to_source(manifest);
	CHECK(anchored.entries[0].directory == "./fonts/");
}

TEST_CASE("a real manifest read from elsewhere names the folders beside itself")
{
	// A manifest in a temporary folder, whose one group says "./fonts/". The
	// process is not in that folder; after the two steps
	// Application::load_manifest takes, the directory the loader would open
	// is the one beside the file.
	const std::filesystem::path folder =
		std::filesystem::temp_directory_path() / "labrador_content_root_test";
	std::filesystem::create_directories(folder);
	const std::filesystem::path file = folder / "manifest.json";
	{
		std::ofstream out(file);
		out << R"({"assets":[{"kind":"font","directory":"./fonts/",)"
			R"("names":["courier"]}]})";
	}

	// An absolute path, so resolving it under the executable leaves it
	// alone. Not asked under Node, whose file system is the host's: a
	// Windows host's temporary directory is a drive path, which a POSIX
	// build rightly does not count as absolute, so the answer would be about
	// the machine running the test. The case above pins what resolving does
	// with a path that is absolute where the build runs.
#if defined(__EMSCRIPTEN__)
	const std::string path = file.string();
#else
	const std::string path = resolved_under(executable_directory(),
		file.string());
	CHECK(path == file.string());
#endif

	const AssetManifest anchored =
		anchored_to_source(labrador::read_asset_manifest(path.c_str()));

	REQUIRE(anchored.entries.size() == 1);
	CHECK(anchored.entries[0].directory ==
		(folder / "./fonts/").string());

	std::filesystem::remove_all(folder);
}


TEST_CASE("UTF-8 manifest and binary content load under non-ANSI directory names")
{
    const std::filesystem::path folder = std::filesystem::temp_directory_path() /
        L"labrador_content_\u6f22_\U0001f680";
    std::filesystem::create_directories(folder / L"textures-\u6f22");
    const std::filesystem::path manifest_file = folder / "manifest.json";
    {
        std::ofstream manifest(manifest_file);
        manifest << R"({"assets":[{"kind":"texture","directory":"textures-\u6f22/","names":["tile"]}]})";
        std::ofstream data(folder / L"textures-\u6f22" / "tile.bin", std::ios::binary);
        data << "binary payload";
    }
    const std::string path = labrador::path_to_utf8(manifest_file);
#if !defined(__EMSCRIPTEN__)
    // Not under Node, for the reason the case above gives.
    CHECK(resolved_under(executable_directory(), path) == path);
#endif
    const AssetManifest manifest = anchored_to_source(labrador::read_asset_manifest(path.c_str()));
    REQUIRE(manifest.entries.size() == 1);
    const std::vector<unsigned char> bytes = labrador::read_file_bytes(
        manifest.entries[0].directory + "tile.bin");
    CHECK(std::string(bytes.begin(), bytes.end()) == "binary payload");
    std::filesystem::remove_all(folder);
}

#include <doctest/doctest.h>

#include "engine/assets/resource_loader.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/colour.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/sprite_sheet.h"
#include "tests/scratch_file.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace labrador;

namespace
{
	void write_sheet(const std::filesystem::path& folder, const std::string& contents)
	{
		std::ofstream file(folder / "atlas.json", std::ios::binary);
		file << contents;
		REQUIRE(file.good());
	}

	void write_atlas(const std::filesystem::path& folder)
	{
		std::vector<unsigned char> bytes(128 + 4 * 2 * 4, 0);
		labrador_tests::put_u32_le(bytes, 0, 0x20534444);
		labrador_tests::put_u32_le(bytes, 4, 124);
		labrador_tests::put_u32_le(bytes, 8, 0x100f);
		labrador_tests::put_u32_le(bytes, 12, 2);
		labrador_tests::put_u32_le(bytes, 16, 4);
		labrador_tests::put_u32_le(bytes, 20, 16);
		labrador_tests::put_u32_le(bytes, 76, 32);
		labrador_tests::put_u32_le(bytes, 80, 0x41);
		labrador_tests::put_u32_le(bytes, 88, 32);
		labrador_tests::put_u32_le(bytes, 92, 0x000000ff);
		labrador_tests::put_u32_le(bytes, 96, 0x0000ff00);
		labrador_tests::put_u32_le(bytes, 100, 0x00ff0000);
		labrador_tests::put_u32_le(bytes, 104, 0xff000000);
		labrador_tests::put_u32_le(bytes, 108, 0x1000);
		std::fill(bytes.begin() + 128, bytes.end(), static_cast<unsigned char>(255));
		std::ofstream file(folder / "atlas.dds", std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		REQUIRE(file.good());
	}

	AssetManifest sheet_manifest(const std::filesystem::path& folder)
	{
		AssetManifest manifest;
		manifest.source_path = "sheet_bounds_manifest.json";
		manifest.entries.push_back(AssetEntry{
			"sprite_sheet", folder.generic_string() + "/", "atlas", false });
		return manifest;
	}
}

TEST_CASE("the sheet loader queries the actual atlas before publishing any frames")
{
	const labrador_tests::ScratchFolder folder("sheet_resource_bounds");
	write_atlas(folder.path());
	const AssetManifest manifest = sheet_manifest(folder.path());
	RenderResources resources;
	Renderer renderer;
	renderer.create_device(nullptr, 64, 64, 1);
	renderer.set_resources(&resources);
	ResourceLoader loader(&resources, &renderer, nullptr, nullptr);
	const std::string valid_frame =
		R"({"name":"edge","position":{"x":4,"y":2},"size":{"w":-4,"h":-2}})";
	const std::string invalid_frame =
		R"({"name":"outside","position":{"x":4,"y":0},"size":{"w":1,"h":1}})";
	const std::string rejected_sheet = R"({"sprite_frames":[)" + valid_frame +
		"," + invalid_frame + R"(],"animation_strips":[]})";
	write_sheet(folder.path(), rejected_sheet);
	CHECK_THROWS_AS(loader.load_manifest(manifest), std::runtime_error);
	CHECK(resources.texture_size(resources.resolve_texture("atlas")) == Vector2I(4, 2));
	CHECK_THROWS_AS(resources.resolve_sprite_sheet("atlas"), std::out_of_range);

	write_sheet(folder.path(), R"({"sprite_frames":[)" + valid_frame +
		R"(],"animation_strips":[]})");
	loader.load_manifest(manifest);
	const RenderResources::SpriteSheetHandle retained = resources.resolve_sprite_sheet("atlas");
	const SpriteSheet* before = resources.sprite_sheet(retained);
	CHECK(before->sprite_frame(before->resolve_sprite_frame("edge"))
		.source_rectangle() == RectangleI(4, 2, -4, -2));

	write_sheet(folder.path(), rejected_sheet);
	CHECK_THROWS_AS(loader.load_manifest(manifest), std::runtime_error);
	CHECK(resources.resolve_sprite_sheet("atlas") == retained);
	CHECK(resources.sprite_sheet(retained) == before);
	CHECK_THROWS_AS(before->resolve_sprite_frame("outside"), std::out_of_range);

	renderer.begin_frame();
	renderer.set_view_count(1);
	DrawList draw = renderer.view(0);
	before->draw(draw, before->resolve_sprite_frame("edge"),
		RectangleF(0.0f, 0.0f, 16.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	renderer.submit();
	const std::vector<RecordedSprite>& sprites = recorded_sprites(renderer);
	REQUIRE(sprites.size() == 1);
	CHECK(sprites[0].corners[0].texcoord == Vector2F(1.0f, 1.0f));
	CHECK(sprites[0].corners[1].texcoord == Vector2F(0.0f, 1.0f));
	CHECK(sprites[0].corners[2].texcoord == Vector2F(1.0f, 0.0f));
	CHECK(sprites[0].corners[3].texcoord == Vector2F(0.0f, 0.0f));
}

#include "samples/components/animation_preview.h"

#include "engine/app/content_root.h"
#include "engine/assets/sprite_sheet_loader.h"
#include "engine/math/rectanglei.h"
#include "engine/render/animation_strip.h"
#include "engine/render/dds_file.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>

using components::AnimationPreview;
using namespace labrador;

// Requires the sample's textures folder staged/embedded beside ComponentsTests.
// Unlike timing fixtures, this checks the delivered DDS and paired loader data.
TEST_CASE("the original animation atlas and JSON load through the shared readers")
{
	const std::string dds_path = resolved_under(executable_directory(),
		"textures/animation_preview.dds");
	const std::string json_path = resolved_under(executable_directory(),
		"textures/animation_preview.json");
	const TextureData atlas = read_dds_file(dds_path.c_str());
	CHECK(atlas.width == 64);
	CHECK(atlas.height == 16);
	CHECK(atlas.format == TextureFormat::r8g8b8a8_unorm);
	REQUIRE(atlas.levels.size() == 1);
	CHECK(atlas.pixels.size() == 64 * 16 * 4);
	std::unique_ptr<SpriteSheet> sheet = read_sprite_sheet(
		json_path.c_str(), TextureHandle());
	const AnimationStrip& loop = sheet->animation_strip(
		sheet->resolve_animation_strip("pulse_loop"));
	CHECK(loop.frame_count() == 4);
	CHECK(loop.frame_time() == 0.25f);
	CHECK(loop.looping());
	CHECK(loop.frame_rect(0) == RectangleI(0, 0, 16, 16));
	CHECK(loop.frame_rect(3) == RectangleI(48, 0, 16, 16));
	RenderResources resources;
	resources.add_sprite_sheet("animation_preview", std::move(sheet));
	AnimationPreview repeating(resources,
		resources.resolve_sheet_strip("animation_preview", "pulse_loop"));
	AnimationPreview once(resources,
		resources.resolve_sheet_strip("animation_preview", "pulse_once"));
	repeating.update(1.0f);
	once.update(1.0f);
	CHECK(repeating.frame_index() == 0);
	CHECK_FALSE(repeating.finished());
	CHECK(once.frame_index() == 3);
	CHECK(once.finished());
}

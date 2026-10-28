#include "samples/components/animation_preview.h"
#include "samples/components/animation_preview_test_content.h"

#include "engine/app/content_root.h"
#include "engine/assets/asset_manifest_loader.h"
#include "engine/assets/resource_loader.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/sprite_sheet.h"
#include "engine/render/texture_data.h"

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

using components::AnimationPreview;
using components::animation_tests::add_sheet;
using namespace labrador;

namespace
{
	// Compile this file only for native null. It uses no window or API device.
	class RecordingContent
	{
	public:
		RecordingContent()
		{
			this->renderer.create_device(nullptr, 160, 90, 2);
			this->renderer.set_resources(&this->resources);
			TextureData atlas;
			atlas.width = 64;
			atlas.height = 16;
			atlas.levels.push_back(texture_level(atlas.format, 64, 16, 0));
			atlas.pixels.assign(atlas.levels[0].size, 255);
			add_texture_asset(this->renderer, this->resources, "preview", atlas);
			this->texture = this->resources.resolve_texture("preview");
			add_sheet(this->resources, "loop", true, 4, 0.25f, this->texture);
			add_sheet(this->resources, "once", false, 4, 0.25f, this->texture);
		}

		// Table before renderer: the renderer dies before the table it borrows.
		RenderResources resources;
		Renderer renderer;
		TextureHandle texture;
	};
}

TEST_CASE("repeated and two-view animation draws record the same frame without advancing")
{
	RecordingContent content;
	AnimationPreview preview(content.resources,
		content.resources.resolve_sheet_strip("loop", "preview"));
	preview.update(0.375f);
	const AnimationPreview& drawing = preview;
	content.renderer.begin_frame();
	content.renderer.set_view_count(2);
	DrawList first = content.renderer.view(0);
	DrawList second = content.renderer.view(1);
	drawing.draw(second, RectangleF(50.0f, 20.0f, 32.0f, 32.0f));
	drawing.draw(first, RectangleF(10.0f, 20.0f, 32.0f, 32.0f));
	drawing.draw(first, RectangleF(10.0f, 20.0f, 32.0f, 32.0f));
	content.renderer.submit();
	const std::vector<RecordedSprite> drawn = recorded_sprites(content.renderer);
	REQUIRE(drawn.size() == 3);
	CHECK(drawn[0].view == 0);
	CHECK(drawn[1].view == 0);
	CHECK(drawn[2].view == 1);
	for (const RecordedSprite& sprite : drawn)
	{
		CHECK(sprite.texture == content.texture);
		CHECK(sprite.corners[0].texcoord == Vector2F(0.25f, 0.0f));
		CHECK(sprite.corners[3].texcoord == Vector2F(0.5f, 1.0f));
	}
	CHECK(drawn[0].corners[0].position == Vector2F(10.0f, 20.0f));
	CHECK(drawn[0].corners[3].position == Vector2F(42.0f, 52.0f));
	CHECK(drawn[1].corners[0].position == drawn[0].corners[0].position);
	CHECK(drawn[2].corners[0].position == Vector2F(50.0f, 20.0f));
	CHECK(preview.elapsed() == 0.375);
	CHECK(preview.frame_index() == 1);
	CHECK_FALSE(preview.finished());

	content.renderer.begin_frame();
	content.renderer.set_view_count(1);
	DrawList again = content.renderer.view(0);
	drawing.draw(again, RectangleF(10.0f, 20.0f, 32.0f, 32.0f));
	content.renderer.submit();
	const std::vector<RecordedSprite> repeated = recorded_sprites(content.renderer);
	REQUIRE(repeated.size() == 1);
	CHECK(repeated[0].corners[0].texcoord == drawn[0].corners[0].texcoord);
	CHECK(preview.elapsed() == 0.375);
	CHECK(preview.frame_index() == 1);

	preview.update(0.375f);
	content.renderer.begin_frame();
	content.renderer.set_view_count(1);
	DrawList advanced = content.renderer.view(0);
	drawing.draw(advanced, RectangleF(10.0f, 20.0f, 32.0f, 32.0f));
	content.renderer.submit();
	const std::vector<RecordedSprite> changed = recorded_sprites(content.renderer);
	REQUIRE(changed.size() == 1);
	CHECK(changed[0].corners[0].texcoord == Vector2F(0.75f, 0.0f));
	CHECK(changed[0].corners[3].texcoord == Vector2F(1.0f, 1.0f));
	CHECK(preview.frame_index() == 3);
}

TEST_CASE("drawing a finished one-shot holds the last frame until restart")
{
	RecordingContent content;
	AnimationPreview preview(content.resources,
		content.resources.resolve_sheet_strip("once", "preview"));
	preview.update(2.0f);
	content.renderer.begin_frame();
	content.renderer.set_view_count(2);
	DrawList first = content.renderer.view(0);
	DrawList second = content.renderer.view(1);
	preview.draw(first, RectangleF(0.0f, 0.0f, 16.0f, 16.0f));
	preview.draw(second, RectangleF(0.0f, 0.0f, 16.0f, 16.0f));
	content.renderer.submit();
	const std::vector<RecordedSprite> held = recorded_sprites(content.renderer);
	REQUIRE(held.size() == 2);
	CHECK(held[0].corners[0].texcoord == Vector2F(0.75f, 0.0f));
	CHECK(held[1].corners[0].texcoord == held[0].corners[0].texcoord);
	CHECK(preview.finished());
	CHECK(preview.elapsed() == 1.0);
	preview.restart();
	content.renderer.begin_frame();
	content.renderer.set_view_count(1);
	DrawList restarted = content.renderer.view(0);
	preview.draw(restarted, RectangleF(0.0f, 0.0f, 16.0f, 16.0f));
	content.renderer.submit();
	const std::vector<RecordedSprite> initial = recorded_sprites(content.renderer);
	REQUIRE(initial.size() == 1);
	CHECK(initial[0].corners[0].texcoord == Vector2F::zero);
	CHECK(initial[0].corners[3].texcoord == Vector2F(0.25f, 1.0f));
	CHECK_FALSE(preview.finished());
	CHECK(preview.elapsed() == 0.0);
}

TEST_CASE("the gallery manifest loads the paired atlas and preserves playback across texture reload")
{
	const std::string manifest_path = resolved_under(executable_directory(),
		"manifest.json");
	const AssetManifest delivered = anchored_to_source(
		read_asset_manifest(manifest_path.c_str()));
	AssetManifest selected;
	selected.source_path = delivered.source_path;
	// Exercise the actual delivered entry and its anchored directory, without
	// making this strip test load unrelated gallery fonts or panel content.
	for (const AssetEntry& entry : delivered.entries)
	{
		if (entry.name == "animation_preview")
		{
			REQUIRE(entry.kind == "sprite_sheet");
			CHECK_FALSE(entry.optional);
			selected.entries.push_back(entry);
		}
	}
	REQUIRE(selected.entries.size() == 1);

	RenderResources resources;
	Renderer renderer;
	renderer.create_device(nullptr, 128, 80, 1);
	renderer.set_resources(&resources);
	// This selection contains only a sprite sheet; no audio service is used.
	ResourceLoader loader(&resources, &renderer, nullptr, nullptr);
	loader.load_manifest(std::move(selected));
	const RenderResources::SheetStrip pair = resources.resolve_sheet_strip(
		"animation_preview", "pulse_loop");
	const SpriteSheet* metadata = resources.sprite_sheet(pair.sheet());
	const TextureHandle texture = resources.resolve_texture("animation_preview");
	CHECK(metadata->texture() == texture);
	AnimationPreview preview(resources, pair);
	preview.update(0.5f);

	renderer.begin_frame();
	renderer.set_view_count(1);
	DrawList first = renderer.view(0);
	preview.draw(first, RectangleF(8.0f, 12.0f, 32.0f, 32.0f));
	renderer.submit();
	const std::vector<RecordedSprite> before = recorded_sprites(renderer);
	REQUIRE(before.size() == 1);
	CHECK(before[0].texture == texture);
	CHECK(before[0].corners[0].texcoord == Vector2F(0.5f, 0.0f));
	CHECK(before[0].corners[3].texcoord == Vector2F(0.75f, 1.0f));

	// A deliberate texture-table reload, not physical device-loss evidence.
	resources.release_device_resources();
	loader.reload_device_resources();
	CHECK(resources.sprite_sheet(pair.sheet()) == metadata);
	CHECK(resources.resolve_texture("animation_preview") == texture);
	renderer.begin_frame();
	renderer.set_view_count(1);
	DrawList reloaded = renderer.view(0);
	preview.draw(reloaded, RectangleF(8.0f, 12.0f, 32.0f, 32.0f));
	renderer.submit();
	const std::vector<RecordedSprite> after = recorded_sprites(renderer);
	REQUIRE(after.size() == 1);
	CHECK(after[0].texture == before[0].texture);
	CHECK(after[0].corners[0].texcoord == before[0].corners[0].texcoord);
	CHECK(after[0].corners[3].texcoord == before[0].corners[3].texcoord);
	CHECK(preview.frame_index() == 2);
	CHECK(preview.elapsed() == 0.5);
	CHECK_FALSE(preview.finished());
}

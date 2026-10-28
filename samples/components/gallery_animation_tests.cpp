#include "samples/components/gallery_state.h"

#include "engine/app/application.h"
#include "engine/input/keyboard.h"
#include "engine/math/vector2f.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"

#include <doctest/doctest.h>
#include <Windows.h>

#include <cstddef>
#include <utility>
#include <vector>

using components::GalleryState;
using namespace labrador;

namespace
{
	std::vector<RecordedSprite> draw_previews(const Application& app,
		const GalleryState& gallery)
	{
		Renderer* renderer = app.renderer();
		renderer->begin_frame();
		gallery.draw(*renderer);
		renderer->submit();
		const TextureHandle atlas =
			app.render_resources()->resolve_texture("animation_preview");
		std::vector<RecordedSprite> previews;
		for (const RecordedSprite& sprite : recorded_sprites(*renderer))
		{
			if (sprite.texture == atlas)
			{
				previews.push_back(sprite);
			}
		}
		return previews;
	}
}

// Native null render AND audio only. The real state uses a hidden window and
// synthetic keyboard snapshots, with no Application loop or API device.
TEST_CASE("the gallery advances both overlays and restarts them on a new R press")
{
	ApplicationOptions options;
	options.window_class_name = L"ComponentsAnimationTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	Application app(std::move(options));
	app.initialize();
	app.load_manifest("manifest.json");
	GalleryState gallery(&app);
	gallery.init();
	CHECK_FALSE(IsWindowVisible(static_cast<HWND>(app.window())));

	gallery.update(0.625f);
	const std::vector<RecordedSprite> first = draw_previews(app, gallery);
	const std::vector<RecordedSprite> repeated = draw_previews(app, gallery);
	REQUIRE(first.size() == 2);
	REQUIRE(repeated.size() == 2);
	for (std::size_t index = 0; index < first.size(); ++index)
	{
		CHECK(first[index].filter == TextureFilter::point);
		CHECK(first[index].view == 0);
		CHECK(first[index].corners[0].texcoord == Vector2F(0.5f, 0.0f));
		CHECK(first[index].corners[3].texcoord == Vector2F(0.75f, 1.0f));
		CHECK(repeated[index].corners[0].texcoord == first[index].corners[0].texcoord);
	}

	gallery.update(0.5f);
	const std::vector<RecordedSprite> after_cycle = draw_previews(app, gallery);
	REQUIRE(after_cycle.size() == 2);
	CHECK(after_cycle[0].corners[0].texcoord == Vector2F::zero);
	CHECK(after_cycle[1].corners[0].texcoord == Vector2F(0.75f, 0.0f));

	Keyboard* keyboard = app.keyboard();
	keyboard->set_focused(true);
	keyboard->poll();
	keyboard->on_key_down(Key::r);
	keyboard->poll();
	gallery.update(0.25f);
	const std::vector<RecordedSprite> restarted = draw_previews(app, gallery);
	REQUIRE(restarted.size() == 2);
	CHECK(restarted[0].corners[0].texcoord == Vector2F::zero);
	CHECK(restarted[1].corners[0].texcoord == Vector2F::zero);

	// Holding R must not repeatedly restart the previews on later snapshots.
	keyboard->poll();
	gallery.update(0.25f);
	const std::vector<RecordedSprite> held = draw_previews(app, gallery);
	REQUIRE(held.size() == 2);
	CHECK(held[0].corners[0].texcoord == Vector2F(0.25f, 0.0f));
	CHECK(held[1].corners[0].texcoord == Vector2F(0.25f, 0.0f));
	CHECK_FALSE(IsWindowVisible(static_cast<HWND>(app.window())));
}

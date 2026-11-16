#include <doctest/doctest.h>

#include "samples/collision/collision_state.h"

#include "engine/app/application.h"
#include "engine/input/keyboard.h"
#include "engine/render/null/recording.h"

#include <cstddef>

using namespace labrador;

TEST_CASE("collision sample owns an edge-triggered outline toggle and const overlay")
{
	ApplicationOptions options;
	options.visible = false;
	options.client_size = Vector2I(1280, 720);
	options.window_class_name = L"CollisionOutlineBindingTests";
	options.max_threads = 1;
	options.view_capacity = 1;
	Application app(options);
	app.initialize();
	app.load_manifest("./manifest.json");
	collision::CollisionState state(&app, false);
	state.init();
	Keyboard& keyboard = *app.keyboard();
	keyboard.set_focused(true);
	keyboard.poll();
	keyboard.poll();
	const auto outline_count = [&app, &state]()
	{
		Renderer& renderer = *app.renderer();
		renderer.begin_frame();
		state.draw(renderer);
		renderer.submit();
		std::size_t count = 0;
		for (const RecordedSprite& sprite : recorded_sprites(renderer))
		{
			if (sprite.corners[0].colour == Colour::yellow) { ++count; }
		}
		return count;
	};
	CHECK(outline_count() == 0);
	keyboard.on_key_down(Key::o);
	keyboard.poll();
	state.update(0.0f);
	const std::size_t shown = outline_count();
	REQUIRE(shown > 0);
	CHECK(outline_count() == shown);
	keyboard.poll();
	state.update(0.0f);
	CHECK(outline_count() == shown);
	keyboard.on_key_up(Key::o);
	keyboard.poll();
	state.update(0.0f);
	keyboard.on_key_down(Key::o);
	keyboard.poll();
	state.update(0.0f);
	CHECK(outline_count() == 0);
}

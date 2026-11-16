#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/scene/scene.h"
#include "samples/collision/entry_marker.h"

#include <memory>

using namespace labrador;
using namespace collision;

TEST_CASE("entry markers draw only after admission and stop drawing after retirement")
{
	RenderResources resources;
	Renderer renderer;
	renderer.create_device(nullptr, 1280, 720, 1);
	renderer.set_resources(&resources);
	add_white_texture(renderer, resources);
	Scene scene(nullptr, nullptr);
	scene.add_view(Viewport(0, 0, 1280, 720));
	const TextureHandle white = resources.resolve_texture(white_texture_name);
	scene.add(std::make_unique<EntryMarker>(Vector2F(200, 300), white));
	const auto draw_count = [&renderer, &scene]()
	{
		renderer.begin_frame();
		scene.draw(renderer);
		renderer.submit();
		const std::size_t count = recorded_sprites(renderer).size();
		renderer.end_frame();
		return count;
	};
	CHECK(draw_count() == 0);
	scene.end_tick();
	REQUIRE(draw_count() == 1);
	const RecordedSprite& sprite = recorded_sprites(renderer)[0];
	CHECK(sprite.texture == white);
	CHECK(sprite.corners[0].position == Vector2F(200, 300));
	CHECK(sprite.corners[3].position == Vector2F(212, 312));
	scene.update(EntryMarker::lifetime);
	CHECK(draw_count() == 1);
	scene.end_tick();
	CHECK(draw_count() == 0);
}

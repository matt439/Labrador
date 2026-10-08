#include <doctest/doctest.h>

#include "engine/collision/partitioner.h"
#include "engine/core/thread_pool.h"
#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/scene/scene.h"
#include "samples/local_multiplayer/arena.h"

#include <memory>

using namespace labrador;
using namespace mattmath;
using namespace multiplayer;

TEST_CASE("both views draw the same shared players and parallel draw preserves the serial recording")
{
	RenderResources resources;
	Renderer renderer;
	renderer.create_device(nullptr, 1280, 720, 2);
	renderer.set_resources(&resources);
	TextureData texture;
	texture.width = texture.height = 1;
	texture.levels.push_back(texture_level(texture.format, 1, 1, 0));
	texture.pixels.assign(4, 255);
	add_texture_asset(renderer, resources, "white", texture);

	ThreadPool pool(1, 2);
	const Partitioner partitioner;
	Scene serial(nullptr, nullptr);
	Scene parallel(&pool, &partitioner);
	const auto populate = [&resources](Scene& scene)
	{
		Arena* arena = scene.add(std::make_unique<Arena>(resources.resolve_texture("white")));
		scene.end_tick();
		arena->set_directions({ Vector2F(1, 0), Vector2F(0, -1) });
		scene.update(0.5f);
		for (size_t player = 0; player < Arena::player_count; ++player)
		{
			const Viewport viewport(0, static_cast<float>(player) * 360.0f, 1280, 360);
			scene.add_view(viewport, arena->camera(player, viewport));
		}
		return arena;
	};
	populate(serial);
	Arena* arena = populate(parallel);
	const Vector2F first_before = arena->position(0);
	const Vector2F second_before = arena->position(1);
	const auto draw = [&renderer](const Scene& scene)
	{
		renderer.begin_frame();
		scene.draw(renderer);
		renderer.submit();
		return std::vector<RecordedSprite>(recorded_sprites(renderer));
	};
	const std::vector<RecordedSprite> expected = draw(serial);
	const std::vector<RecordedSprite> actual = draw(parallel);
	REQUIRE(actual.size() == expected.size());
	for (size_t index = 0; index < actual.size(); ++index)
	{
		CHECK(actual[index].view == expected[index].view);
		CHECK(actual[index].viewport == expected[index].viewport);
		for (size_t corner = 0; corner < 4; ++corner)
		{
			CHECK(actual[index].corners[corner].position == expected[index].corners[corner].position);
			CHECK(actual[index].corners[corner].colour == expected[index].corners[corner].colour);
		}
	}
	int first_count = 0;
	int second_count = 0;
	for (const RecordedSprite& sprite : actual)
	{
		if (sprite.corners[0].colour == Arena::player_colour(0))
		{
			++first_count;
			if (sprite.view == 0) CHECK(sprite.corners[0].position == Vector2F(626, 166));
		}
		if (sprite.corners[0].colour == Arena::player_colour(1))
		{
			++second_count;
			if (sprite.view == 1) CHECK(sprite.corners[0].position == Vector2F(626, 166));
		}
	}
	CHECK(first_count == 2);
	CHECK(second_count == 2);
	CHECK(arena->position(0) == first_before);
	CHECK(arena->position(1) == second_before);
}

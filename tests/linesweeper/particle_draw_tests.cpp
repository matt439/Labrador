#include <doctest/doctest.h>

#include "engine/render/null/recording.h"
#include "engine/render/render_resources.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/particles.h"

#include <algorithm>
#include <limits>
#include <memory>

TEST_CASE("particle bounds contain visible edges and survive an edge-view cull")
{
	using namespace labrador;
	using namespace linesweeper;
	using namespace mattmath;
	RenderResources resources;
	Renderer renderer;
	renderer.create_device(nullptr, 1280, 720, 1);
	renderer.set_resources(&resources);
	TextureData texture;
	texture.width = texture.height = 1;
	texture.levels.push_back(texture_level(texture.format, 1, 1, 0));
	texture.pixels.assign(4, 255);
	add_texture_asset(renderer, resources, "white", texture);
	World world;
	TickResult tick;
	world.cells[static_cast<size_t>(cell_index(0, well_rows - 1))] = 1;
	Scene scene(nullptr, nullptr);
	ParticleField* field = scene.add(std::make_unique<ParticleField>(&world, &tick,
		resources.resolve_texture("white")));
	scene.end_tick();
	world.topped_out = 1;
	++world.tick;
	field->update(0.0f);
	renderer.begin_frame();
	renderer.set_view_count(1);
	DrawList list = renderer.view(0);
	field->draw(list);
	renderer.submit();
	REQUIRE_FALSE(recorded_sprites(renderer).empty());
	float left = (std::numeric_limits<float>::max)();
	const RectangleF cull = field->cull_bounds(1);
	for (const RecordedSprite& sprite : recorded_sprites(renderer))
	for (const SpriteVertex& corner : sprite.corners)
	{
		left = std::min(left, corner.position.x);
		CHECK(corner.position.x >= cull.left());
		CHECK(corner.position.x <= cull.right());
		CHECK(corner.position.y >= cull.top());
		CHECK(corner.position.y <= cull.bottom());
	}
	// A world-space edge and its pixel-truncated counterpart differ by <1.
	CHECK(field->bounds().left() < left + 1.0f);
	scene.add_view(Viewport(0, 0, 1, 720), Camera(left, 0, 1));
	renderer.begin_frame();
	scene.draw(renderer);
	renderer.submit();
	CHECK_FALSE(recorded_sprites(renderer).empty());
	field->update(10.0f);
	CHECK(field->live() == 0);
	CHECK(field->cull_bounds(100).width == 0);
}

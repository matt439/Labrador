#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/render/resolution_manager.h"
#include "engine/render/viewport_manager.h"
#include "engine/scene/scene.h"
#include "samples/local_multiplayer/arena.h"
#include "samples/local_multiplayer/controls.h"

#include <memory>

using namespace labrador;
using namespace mattmath;
using namespace multiplayer;

TEST_CASE("keyboard bindings route simultaneous input to different players without controllers")
{
	Keyboard keyboard;
	keyboard.set_focused(true);
	keyboard.on_key_down(Key::d);
	keyboard.on_key_down(Key::up);
	keyboard.poll();
	const std::array<Vector2F, Arena::player_count> sticks = {};
	const std::array<Vector2F, Arena::player_count> directions = read_directions(keyboard, sticks);
	CHECK(directions[0] == Vector2F(1.0f, 0.0f));
	CHECK(directions[1] == Vector2F(0.0f, -1.0f));
	keyboard.on_key_up(Key::d);
	keyboard.poll();
	CHECK(read_directions(keyboard, sticks)[0] == Vector2F::ZERO);
	CHECK(read_directions(keyboard, sticks)[1] == Vector2F(0.0f, -1.0f));
}

TEST_CASE("stick ownership is per player and deadzone is applied before movement")
{
	Keyboard keyboard;
	const std::array<Vector2F, Arena::player_count> sticks = {
		Vector2F(0.1f, 0.0f), Vector2F(0.0f, 1.0f)
	};
	const std::array<Vector2F, Arena::player_count> directions = read_directions(keyboard, sticks);
	CHECK(directions[0] == Vector2F::ZERO);
	CHECK(directions[1] == Vector2F(0.0f, 1.0f));
}

TEST_CASE("one scene tick moves each player once regardless of the view count")
{
	Scene scene(nullptr, nullptr);
	Arena* arena = scene.add(std::make_unique<Arena>(TextureHandle{}));
	scene.end_tick();
	scene.add_view(Viewport(0, 0, 1280, 360));
	scene.add_view(Viewport(0, 360, 1280, 360));
	arena->set_directions({ Vector2F(1, 0), Vector2F(0, -1) });
	scene.update(0.5f);
	CHECK(arena->position(0) == Vector2F(840, 540));
	CHECK(arena->position(1) == Vector2F(960, 420));
}

TEST_CASE("combined devices and diagonals share one speed cap and stop at world edges")
{
	Arena arena(TextureHandle{});
	const Vector2F start = arena.position(0);
	arena.set_directions({ Vector2F(2, 1), Vector2F(-1, -1) });
	arena.update(1.0f);
	CHECK((arena.position(0) - start).length() == doctest::Approx(240.0f));
	arena.update(100.0f);
	CHECK(arena.position(0) == Vector2F(1786, 1066));
	CHECK(arena.position(1) == Vector2F(14, 14));
}

TEST_CASE("each camera centres its own player in pane coordinates after a resize")
{
	Arena arena(TextureHandle{});
	ResolutionManager resolution;
	ViewportManager viewports(&resolution);
	viewports.set_layout(ScreenLayout::two_player);
	for (const Vector2F size : { Vector2F(1280, 720), Vector2F(1000, 801) })
	{
		resolution.set_resolution_exactly(Vector2I(static_cast<int>(size.x), static_cast<int>(size.y)));
		for (size_t player = 0; player < Arena::player_count; ++player)
		{
			const Viewport viewport = viewports.player_viewport(static_cast<int>(player));
			const Camera camera = arena.camera(player, viewport);
			CHECK(camera.calculate_view_position(arena.position(player)) == viewport.size() * 0.5f);
			CHECK(viewport.y == (player == 0 ? 0.0f : size.y * 0.5f));
		}
	}
}

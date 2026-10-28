#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/render/null/recording.h"
#include "samples/local_multiplayer/join_state.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

// The real screen against synthetic keyboard input. Application supplies a
// hidden window and null services; no application loop or pad polling runs.
namespace
{
	using namespace labrador;
	using namespace multiplayer;

	ApplicationOptions options()
	{
		ApplicationOptions result;
		result.window_class_name = L"LocalMultiplayerJoinStateTests";
		result.visible = false;
		result.view_capacity = 2;
		result.max_threads = 1;
		return result;
	}

	class Menu
	{
	public:
		Menu() : app_(options())
		{
			this->app_.initialize();
			// The menu uses the minimal sample's font. Load its source-tree
			// manifest instead of relying on the executable's copied content.
			this->app_.load_manifest(LOCAL_MULTIPLAYER_MANIFEST);
			this->app_.keyboard()->set_focused(true);
			this->app_.keyboard()->poll();
			this->app_.keyboard()->poll();
			std::unique_ptr<JoinState> state = std::make_unique<JoinState>(&this->app_);
			this->state_ = state.get();
			this->context_.transition_to(std::move(state));
		}

		void step()
		{
			this->app_.keyboard()->poll();
			this->context_.update(1.0f / 60.0f);
		}

		void press(Key key)
		{
			this->hold(key, true);
			this->hold(key, false);
		}

		void hold(Key key, bool down)
		{
			if (down) this->app_.keyboard()->on_key_down(key);
			else this->app_.keyboard()->on_key_up(key);
			this->step();
		}

		const Lobby& lobby() const { return this->state_->lobby(); }

		std::vector<RecordedSprite> draw_at(const Vector2I& size)
		{
			this->app_.resolution_manager()->set_resolution_exactly(size);
			this->step();
			Renderer& renderer = *this->app_.renderer();
			renderer.begin_frame();
			this->context_.draw(renderer);
			renderer.submit();
			return recorded_sprites(renderer);
		}

	private:
		Application app_;
		StateContext context_;
		JoinState* state_ = nullptr;
	};
}

TEST_CASE("couch menu routes each keyboard half to its assigned focus slot")
{
	Menu menu;
	menu.press(Key::enter);
	menu.press(Key::f);
	CHECK(menu.lobby().players[0].source == InputSource::keyboard_right);
	CHECK(menu.lobby().players[1].source == InputSource::keyboard_left);
	CHECK_FALSE(menu.lobby().players[0].ready);
	CHECK_FALSE(menu.lobby().players[1].ready);
	menu.press(Key::enter);
	CHECK(menu.lobby().players[0].ready);
	menu.press(Key::s);
	menu.press(Key::f);
	CHECK(menu.lobby().players[1].source == InputSource::none);
	CHECK(menu.lobby().players[0].source == InputSource::keyboard_right);
	CHECK(menu.lobby().players[0].ready);
}

TEST_CASE("a direction held while joining cannot move that player's first menu choice")
{
	Menu menu;
	menu.hold(Key::s, true);
	menu.press(Key::f);
	for (int frame = 0; frame < 40; ++frame) menu.step();
	menu.press(Key::f);
	CHECK(menu.lobby().players[0].source == InputSource::keyboard_left);
	CHECK(menu.lobby().players[0].ready);
}

TEST_CASE("couch menu widgets and instructions fit both panes after a resize")
{
	Menu menu;
	menu.press(Key::f);
	for (const Vector2I size : { Vector2I(1280, 720), Vector2I(320, 200), Vector2I(1000, 801) })
	{
		const std::vector<RecordedSprite> sprites = menu.draw_at(size);
		REQUIRE_FALSE(sprites.empty());
		for (const RecordedSprite& sprite : sprites)
		{
			CHECK(sprite.view >= 0);
			CHECK(sprite.view < 2);
			for (const SpriteVertex& corner : sprite.corners)
			{
				CHECK(corner.position.x >= 0.0f);
				CHECK(corner.position.y >= 0.0f);
				CHECK(corner.position.x <= sprite.viewport.width);
				CHECK(corner.position.y <= sprite.viewport.height);
			}
		}
	}
}

TEST_CASE("both couch players choosing ready replaces the menu with the shared arena")
{
	Menu menu;
	menu.press(Key::f);
	menu.press(Key::enter);
	menu.press(Key::f);
	menu.press(Key::enter);
	// JoinState has now been destroyed by the transition. Read the new
	// state's recorded drawing rather than dereferencing the old menu.
	const std::vector<RecordedSprite> sprites = menu.draw_at(Vector2I(1280, 720));
	std::array<int, Arena::player_count> landmarks = {};
	for (const RecordedSprite& sprite : sprites)
	{
		if (sprite.corners[0].colour == Colour(129, 103, 186))
		{
			landmarks.at(static_cast<std::size_t>(sprite.view))++;
		}
	}
	CHECK(landmarks[0] == 2);
	CHECK(landmarks[1] == 2);
}

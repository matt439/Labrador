// The first screen driven by keys from a compiled test: the real HelloState
// and ConfirmState, in an Application with a hidden window, in a build whose
// renderer and audio are null. No graphics device, audio endpoint or
// application loop is used, and no key reaches the window: the test hands the
// keyboard its key events itself, as the window does. The build file defines
// GAME_MANIFEST as the content folder's manifest.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "samples/minimal/states/hello_state.h"

#include <memory>

using namespace labrador;

namespace
{
	ApplicationOptions hidden_options()
	{
		ApplicationOptions options;
		options.window_class_name = L"GameStateTests";
		options.visible = false;
		options.view_capacity = 1;
		options.max_threads = 1;
		return options;
	}

	// One game, stepped by hand. The Application supplies the services a
	// state reads - the keyboard, the fonts - and its own loop never runs, so
	// the stack here is a StateContext of the test's own.
	class Game
	{
	public:
		Game() : app_(hidden_options())
		{
			this->app_.initialize();
			this->app_.load_manifest(GAME_MANIFEST);

			// A hidden window never has the keyboard, so the test says it does.
			// Focus is latched by poll(), like keys.
			this->app_.keyboard()->set_focused(true);
			this->app_.keyboard()->poll();
			this->context_.transition_to(std::make_unique<HelloState>(&this->app_));
		}

		// One update, as the loop runs it: the keyboard is polled first, so
		// what was fed since the last step is what this one sees.
		void step()
		{
			this->app_.keyboard()->poll();
			this->context_.update(1.0f / 60.0f);
		}

		void hold(Key key)
		{
			this->app_.keyboard()->on_key_down(key);
			this->step();
		}

		void release(Key key)
		{
			this->app_.keyboard()->on_key_up(key);
			this->step();
		}

		// Down for one step and up for the next, so a poll sees it held.
		void press(Key key)
		{
			this->hold(key);
			this->release(key);
		}

		// Down and up again before the next poll, as a key sent faster than
		// an update arrives.
		void tap_between_polls(Key key)
		{
			this->app_.keyboard()->on_key_down(key);
			this->app_.keyboard()->on_key_up(key);
			this->step();
		}

		// How many states are on the stack: 1 for HelloState alone, 2 while
		// its quit question is open over it.
		int depth() const { return this->context_.depth(); }

	private:
		Application app_;
		StateContext context_;
	};
}

TEST_CASE("Escape asks before quitting, and a second Escape takes the question back")
{
	Game game;
	game.step();
	CHECK(game.depth() == 1);

	game.press(Key::escape);
	CHECK(game.depth() == 2);

	game.press(Key::escape);
	CHECK(game.depth() == 1);
}

TEST_CASE("the Escape that opens the question does not also close it")
{
	Game game;
	game.step();

	// Held for a second of steps: the question opens on the first and stays,
	// because it answers a new press, not a key that is still down.
	game.hold(Key::escape);
	for (int step = 0; step < 60; ++step)
	{
		game.step();
	}
	CHECK(game.depth() == 2);

	game.release(Key::escape);
	CHECK(game.depth() == 2);
}

TEST_CASE("a key pressed and released between two polls is never seen")
{
	Game game;
	game.step();

	game.tap_between_polls(Key::escape);
	CHECK(game.depth() == 1);
}

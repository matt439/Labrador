#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "samples/linesweeper/states/play_state.h"

#include <memory>
#include <string>

// Real sample states and input polling, with a hidden window and both null
// backends. The independent context receives synthetic activation notifications;
// no application loop, browser, graphics device or physical input is exercised.
namespace
{
	using namespace labrador;
	using namespace linesweeper;

	ApplicationOptions options()
	{
		ApplicationOptions result;
		result.window_class_name = L"LineSweeperFocusPauseTests";
		result.visible = false;
		result.view_capacity = 1;
		result.max_threads = 1;
		return result;
	}

	class Match
	{
	public:
		explicit Match(bool active = true) : app_(options())
		{
			this->app_.initialize();
			this->app_.load_manifest(std::string(LINESWEEPER_CONTENT_DIRECTORY) + "manifest.json");
			this->app_.keyboard()->set_focused(true);
			this->app_.keyboard()->poll();
			this->app_.keyboard()->poll();
			this->context_.notify_activation(active);
			this->context_.transition_to(std::make_unique<PlayState>(&this->app_));
		}

		void step()
		{
			this->app_.keyboard()->poll();
			this->context_.update(1.0f / 60.0f);
		}

		void press(Key key)
		{
			this->app_.keyboard()->on_key_down(key);
			this->step();
			this->app_.keyboard()->on_key_up(key);
			this->step();
		}

		void focus(bool focused)
		{
			this->app_.keyboard()->set_focused(focused);
			this->step();
		}

		void activate(bool active)
		{
			this->context_.notify_activation(active);
		}

		int depth() const { return this->context_.depth(); }

	private:
		Application app_;
		StateContext context_;
	};
}

TEST_CASE("LineSweeper pauses on keyboard focus loss and waits for an explicit resume")
{
	Match match;
	match.step();
	CHECK(match.depth() == 1);
	match.focus(false);
	CHECK(match.depth() == 2);
	match.step();
	CHECK(match.depth() == 2);
	match.focus(true);
	CHECK(match.depth() == 2);
	match.press(Key::escape);
	CHECK(match.depth() == 1);
	match.focus(false);
	CHECK(match.depth() == 2);
}

TEST_CASE("LineSweeper pauses on deactivation even when no update runs before returning")
{
	Match match;
	match.step();
	match.activate(false);
	CHECK(match.depth() == 2);
	match.activate(true);
	CHECK(match.depth() == 2);
	match.press(Key::escape);
	CHECK(match.depth() == 1);
}

TEST_CASE("LineSweeper does not stack pause menus when a paused match loses focus")
{
	Match match;
	match.step();
	match.press(Key::escape);
	CHECK(match.depth() == 2);
	match.activate(false);
	match.activate(false);
	match.activate(true);
	match.focus(false);
	match.focus(true);
	CHECK(match.depth() == 2);
	match.press(Key::escape);
	CHECK(match.depth() == 1);
	match.activate(false);
	CHECK(match.depth() == 2);
}

TEST_CASE("LineSweeper initialized while inactive starts in its pause menu")
{
	Match match(false);
	CHECK(match.depth() == 2);
	match.activate(true);
	CHECK(match.depth() == 2);
	match.press(Key::escape);
	CHECK(match.depth() == 1);
}

TEST_CASE("LineSweeper keeps its top-out screen when focus is lost")
{
	Match match;
	match.step();
	// Unrotated pieces land in the same narrow columns, so no complete row
	// clears and these drops fill the well regardless of queue order.
	for (int drop = 0; drop < 32; ++drop)
	{
		match.press(Key::space);
	}
	match.press(Key::escape);
	CHECK(match.depth() == 1);
	match.focus(false);
	CHECK(match.depth() == 1);
	match.activate(false);
	CHECK(match.depth() == 1);
}

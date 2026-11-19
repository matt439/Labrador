#include <doctest/doctest.h>

#include "samples/components/touch_demo_state.h"
#include "samples/components/gallery_state.h"

#include "engine/app/application.h"
#include "engine/core/state_context.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/scripted_gamepad_source.h"
#include "engine/input/touch.h"
#include "engine/render/null/recording.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace labrador;
using components::TouchDemoReading;
using components::TouchDemoState;

namespace
{
	class EmptyRoot final : public State
	{
	public:
		void init() override {}
		void update(float) override {}
		void draw(Renderer&) const override {}
	};

	ApplicationOptions hidden_options(ScriptedGamepadSource& pads)
	{
		ApplicationOptions options;
		options.window_class_name = L"ComponentsHeldTouchTests";
		options.visible = false;
		options.view_capacity = 1;
		options.max_threads = 1;
		options.scripted_gamepad_source = &pads;
		return options;
	}

	class Demo
	{
	public:
		explicit Demo(bool gallery = false) : app_(hidden_options(this->pads_))
		{
			this->app_.initialize();
			this->app_.load_manifest("manifest.json");
			this->app_.keyboard()->set_focused(true);
			this->app_.mouse()->set_focused(true);
			this->app_.touch()->set_focused(true);
			this->poll();
			this->poll();
			if (gallery)
			{
				this->app_.transition_to(std::make_unique<components::GalleryState>(
					&this->app_, [this](const std::wstring& status) { this->heard_.push_back(status); }));
			}
			else
			{
				this->app_.transition_to(std::make_unique<EmptyRoot>());
				std::unique_ptr<TouchDemoState> page = std::make_unique<TouchDemoState>(
					&this->app_, [this](const std::wstring& status) { this->heard_.push_back(status); });
				this->page_ = page.get();
				this->app_.push(std::move(page));
			}
			this->step();
		}

		Application& app() { return this->app_; }
		ScriptedGamepadSource& pads() { return this->pads_; }
		int depth() const { return this->app_.depth(); }
		const std::vector<std::wstring>& heard() const { return this->heard_; }
		TouchDemoReading reading() const
		{
			if (this->depth() != 2) throw std::logic_error("touch page is no longer live");
			return this->page_->reading();
		}
		Vector2I client(float x, float y) const
		{
			const Viewport viewport(RectangleF(Vector2F::zero,
				this->app_.resolution_manager()->resolution_vec()));
			const Camera camera = Camera::frame(RectangleF(0, 0, 1280, 720), viewport);
			const Vector2F position = camera.calculate_view_position(Vector2F(x, y));
			return Vector2I(static_cast<int>(std::lround(position.x)),
				static_cast<int>(std::lround(position.y)));
		}
		void down(std::uint32_t platform_id, float x, float y)
		{
			this->app_.touch()->on_down(platform_id, this->client(x, y));
		}
		void move(std::uint32_t platform_id, float x, float y)
		{
			this->app_.touch()->on_move(platform_id, this->client(x, y));
		}
		void up(std::uint32_t platform_id, float x, float y)
		{
			this->app_.touch()->on_up(platform_id, this->client(x, y));
		}
		void step()
		{
			this->poll();
			static_cast<StateContext&>(this->app_).update(1.0f / 60.0f);
		}
		void key(Key key)
		{
			this->app_.keyboard()->on_key_down(key);
			this->step();
			this->app_.keyboard()->on_key_up(key);
			this->step();
		}
		void click(float x, float y)
		{
			this->app_.mouse()->on_move(this->client(x, y));
			this->app_.mouse()->on_button_down(MouseButton::left);
			this->step();
			this->app_.mouse()->on_button_up(MouseButton::left);
			this->step();
		}
		std::vector<RecordedSprite> draw()
		{
			Renderer& renderer = *this->app_.renderer();
			renderer.begin_frame();
			static_cast<const StateContext&>(this->app_).draw(renderer);
			renderer.submit();
			renderer.end_frame();
			return recorded_sprites(renderer);
		}
	private:
		void poll()
		{
			this->app_.gamepads()->poll();
			this->app_.keyboard()->poll();
			this->app_.mouse()->poll();
			this->app_.touch()->poll();
		}
		std::vector<std::wstring> heard_;
		ScriptedGamepadSource pads_;
		Application app_;
		TouchDemoState* page_ = nullptr;
	};
}

TEST_CASE("the gallery touch page opens separately and refuses F2 during either modal and its exit tick")
{
	Demo demo(true);
	demo.key(Key::f2);
	REQUIRE(demo.depth() == 2);
	demo.key(Key::escape);
	REQUIRE(demo.depth() == 1);
	demo.step();
	demo.click(1000, 190);
	REQUIRE(demo.depth() == 2);
	demo.key(Key::escape);
	demo.step();

	SUBCASE("Name editing")
	{
		demo.click(980, 510);
		REQUIRE(demo.heard().back() == L"Name editing: Player");
		demo.key(Key::f2);
		CHECK(demo.depth() == 1);
		demo.app().keyboard()->on_key_down(Key::enter);
	}
	SUBCASE("Binding capture")
	{
		demo.click(200, 500);
		REQUIRE(demo.heard().back().starts_with(L"Binding capture:"));
		demo.key(Key::f2);
		CHECK(demo.depth() == 1);
		demo.app().keyboard()->on_key_down(Key::escape);
	}
	demo.app().keyboard()->on_key_down(Key::f2);
	demo.step();
	CHECK(demo.depth() == 1);
	demo.app().keyboard()->on_key_up(Key::enter);
	demo.app().keyboard()->on_key_up(Key::escape);
	demo.app().keyboard()->on_key_up(Key::f2);
	demo.step();
	CHECK(demo.depth() == 1);
	demo.key(Key::f2);
	CHECK(demo.depth() == 2);
}

TEST_CASE("a touch-entry click cancels a gallery modal without opening on its later release")
{
	Demo demo(true);
	SUBCASE("Name")
	{
		demo.click(980, 510);
		REQUIRE(demo.heard().back() == L"Name editing: Player");
	}
	SUBCASE("Binding")
	{
		demo.click(200, 500);
		REQUIRE(demo.heard().back().starts_with(L"Binding capture:"));
	}
	demo.click(1000, 190);
	CHECK(demo.depth() == 1);
	demo.click(1000, 190);
	CHECK(demo.depth() == 2);
}

TEST_CASE("the first resumed gallery frame restores its own cursor after the touch page Back")
{
	Demo demo(true);
	demo.key(Key::f2);
	REQUIRE(demo.depth() == 2);
	demo.click(1100, 88);
	CHECK(demo.depth() == 1);
	CHECK(demo.app().cursor_shape() == CursorShape::arrow);
}

TEST_CASE("returning from the touch page discards old gallery drags and still-held fingers")
{
	Demo demo(true);
	Mouse& mouse = *demo.app().mouse();
	mouse.on_move(demo.client(630, 349));
	mouse.on_button_down(MouseButton::left);
	demo.step();
	REQUIRE(demo.heard().back().starts_with(L"Music slider example: 50%"));
	demo.key(Key::f2);
	REQUIRE(demo.depth() == 2);
	mouse.on_button_up(MouseButton::left);
	demo.step();
	demo.down(10, 490, 250);
	demo.step();
	demo.key(Key::escape);
	REQUIRE(demo.depth() == 1);
	const std::size_t reports = demo.heard().size();
	demo.move(10, 500, 250);
	demo.step();
	demo.up(10, 500, 250);
	demo.step();
	demo.step();
	demo.step();
	CHECK(demo.heard().size() == reports);
	demo.click(490, 250);
	CHECK(demo.heard().back() == L"Difficulty is now Easy");
}

TEST_CASE("the touch demo routes simultaneous fingers without transferring a held contact")
{
	Demo demo;
	demo.down(10, 290, 390);
	demo.down(20, 800, 360);
	demo.down(30, 1010, 360);
	demo.step();
	CHECK(demo.reading().a_held);
	CHECK(demo.reading().b_held);
	CHECK(demo.reading().stick == Vector2F::zero);
	demo.move(10, 590, 390);
	demo.down(40, 800, 360);
	demo.step();
	CHECK(demo.reading().stick == Vector2F(1, 0));
	demo.move(20, 700, 360);
	demo.step();
	CHECK_FALSE(demo.reading().a_held);
	CHECK(demo.reading().b_held);
	demo.move(40, 810, 360);
	demo.step();
	CHECK_FALSE(demo.reading().a_held);
	demo.move(20, 800, 360);
	demo.step();
	CHECK(demo.reading().a_held);
	demo.up(20, 800, 360);
	demo.step();
	CHECK_FALSE(demo.reading().a_held);
	CHECK(demo.reading().b_held);
	demo.down(50, 800, 360);
	demo.step();
	CHECK(demo.reading().a_held);
}

TEST_CASE("cancelling one touch control preserves the other fingers and adopts none")
{
	Demo demo;
	demo.down(10, 290, 390);
	demo.down(20, 800, 360);
	demo.down(30, 1010, 360);
	demo.step();
	demo.move(10, 590, 390);
	demo.down(40, 800, 360);
	demo.step();
	demo.app().touch()->on_cancel(20);
	demo.step();
	CHECK_FALSE(demo.reading().a_held);
	CHECK(demo.reading().b_held);
	CHECK(demo.reading().stick == Vector2F(1, 0));
	demo.move(40, 810, 360);
	demo.step();
	CHECK_FALSE(demo.reading().a_held);
	CHECK(demo.depth() == 2);
	demo.app().touch()->cancel_points();
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
}

TEST_CASE("a canvas blur and focus between polls cancels touch despite retained raw points")
{
	Demo demo;
	demo.down(10, 290, 390);
	demo.down(20, 800, 360);
	demo.step();
	demo.move(10, 590, 390);
	demo.step();
	WindowNotify& notify = demo.app();
	notify.on_keyboard_lost();
	notify.on_keyboard_gained();
	demo.step();
	CHECK(demo.app().touch()->focused());
	CHECK(demo.app().touch()->points().size() == 2);
	CHECK(demo.reading() == TouchDemoReading{});
	demo.move(20, 800, 360);
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
	demo.up(10, 590, 390);
	demo.up(20, 800, 360);
	demo.step();
	demo.down(30, 800, 360);
	demo.step();
	CHECK(demo.reading().a_held);
}

TEST_CASE("whole-window focus loss and return never replay a touch hold")
{
	Demo demo;
	demo.down(20, 800, 360);
	demo.step();
	WindowNotify& notify = demo.app();
	notify.on_deactivated();
	notify.on_activated();
	demo.step();
	CHECK(demo.app().touch()->points().empty());
	CHECK(demo.reading() == TouchDemoReading{});
	demo.move(20, 800, 360);
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
	demo.down(30, 800, 360);
	demo.step();
	CHECK(demo.reading().a_held);
}

TEST_CASE("touch-demo resize cancels ownership until all fingers have cleared")
{
	Demo demo;
	demo.down(20, 800, 360);
	demo.step();
	REQUIRE(demo.reading().a_held);
	demo.app().set_resolution(Vector2I(1000, 800));
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
	demo.move(20, 800, 360);
	demo.down(30, 1010, 360);
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
	demo.up(20, 800, 360);
	demo.up(30, 1010, 360);
	demo.step();
	demo.down(40, 800, 360);
	demo.step();
	CHECK(demo.reading().a_held);
}

TEST_CASE("touch takes over a mouse preview without adopting its held left button later")
{
	Demo demo;
	Mouse& mouse = *demo.app().mouse();
	mouse.on_move(demo.client(800, 360));
	mouse.on_button_down(MouseButton::left);
	demo.step();
	CHECK(demo.reading().a_held);
	demo.down(30, 1010, 360);
	demo.step();
	CHECK_FALSE(demo.reading().a_held);
	CHECK(demo.reading().b_held);
	demo.up(30, 1010, 360);
	demo.step();
	demo.step();
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
	mouse.on_button_up(MouseButton::left);
	demo.step();
	mouse.on_button_down(MouseButton::left);
	demo.step();
	CHECK(demo.reading().a_held);
	mouse.cancel_buttons();
	demo.step();
	CHECK(demo.reading() == TouchDemoReading{});
	CHECK(demo.depth() == 2);
}

TEST_CASE("the touch demo Back action requires a normal release after its last sampled inside position")
{
	Demo demo;
	demo.down(10, 1100, 88);
	demo.step();
	demo.app().touch()->on_cancel(10);
	demo.step();
	CHECK(demo.depth() == 2);
	demo.down(20, 1100, 88);
	demo.step();
	demo.move(20, 980, 88);
	demo.step();
	demo.up(20, 980, 88);
	demo.step();
	CHECK(demo.depth() == 2);
	demo.down(30, 1100, 88);
	demo.step();
	demo.up(30, 1100, 88);
	demo.step();
	CHECK(demo.depth() == 1);
	CHECK(demo.app().window() != nullptr);
}

TEST_CASE("touch-demo Back uses the last snapshot when an outside move and up occur between polls")
{
	Demo demo;
	demo.down(10, 1100, 88);
	demo.step();
	demo.move(10, 980, 88);
	demo.up(10, 980, 88);
	// Touch snapshots retain no final-up position. Unlike a sampled outside
	// move above, this unseen movement cannot invalidate the sampled hold.
	demo.step();
	CHECK(demo.depth() == 1);
}

TEST_CASE("touch-demo Escape and the existing pad-B binding return without closing the gallery")
{
	Demo demo;
	SUBCASE("Escape")
	{
		demo.app().keyboard()->on_key_down(Key::escape);
		demo.step();
	}
	SUBCASE("scripted pad B")
	{
		GamepadState pad;
		pad.connected = true;
		demo.pads().set_state(0, pad);
		demo.step();
		pad.buttons = gamepad_button_bit(GamepadButton::b);
		demo.pads().set_state(0, pad);
		demo.step();
	}
	CHECK(demo.depth() == 1);
	CHECK(demo.app().window() != nullptr);
}

TEST_CASE("touch-demo drawing reads the bound controls without changing or reporting input")
{
	Demo demo;
	demo.down(10, 290, 390);
	demo.down(20, 800, 360);
	demo.down(30, 1010, 360);
	demo.step();
	demo.move(10, 590, 390);
	demo.step();
	const TouchDemoReading before = demo.reading();
	const std::size_t reports = demo.heard().size();
	const std::vector<RecordedSprite> first = demo.draw();
	const std::vector<RecordedSprite> second = demo.draw();
	REQUIRE_FALSE(first.empty());
	REQUIRE(first.size() == second.size());
	for (std::size_t index = 0; index < first.size(); ++index)
	{
		for (std::size_t corner = 0; corner < 4; ++corner)
		{
			CHECK(first[index].corners[corner].position == second[index].corners[corner].position);
			CHECK(first[index].corners[corner].colour == second[index].corners[corner].colour);
		}
	}
	CHECK(demo.reading() == before);
	CHECK(demo.heard().size() == reports);
}

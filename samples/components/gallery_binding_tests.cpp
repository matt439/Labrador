#include <doctest/doctest.h>

#include "samples/components/gallery_state.h"

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/scripted_gamepad_source.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/camera.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/viewport.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

using components::GalleryState;
using namespace labrador;

namespace
{
	ApplicationOptions binding_options(ScriptedGamepadSource* source)
	{
		ApplicationOptions options;
		options.window_class_name = L"ComponentsBindingTestWindow";
		options.visible = false;
		options.view_capacity = 1;
		options.max_threads = 1;
		options.scripted_gamepad_source = source;
		return options;
	}

	struct GalleryFixture
	{
		std::vector<std::wstring> heard;
		ScriptedGamepadSource source;
		Application app;
		GalleryState gallery;
		HWND window = nullptr;

		GalleryFixture() :
			app(binding_options(&this->source)),
			gallery(&app, [this](const std::wstring& text) { this->heard.push_back(text); })
		{
			this->app.initialize();
			this->app.load_manifest("manifest.json");
			this->gallery.init();
			this->window = static_cast<HWND>(this->app.window());
			REQUIRE(IsWindow(this->window));
			CHECK_FALSE(IsWindowVisible(this->window));
			REQUIRE_FALSE(this->heard.empty());
			this->app.keyboard()->set_focused(true);
			this->app.mouse()->set_focused(true);
			this->app.touch()->set_focused(true);
			this->tick();
		}

		~GalleryFixture()
		{
			// A test that intentionally closes its hidden window leaves WM_QUIT
			// for a pump that is not running here. Do not pass it to another case.
			MSG message{};
			while (PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
		}

		void tick()
		{
			this->app.mouse()->poll();
			this->app.touch()->poll();
			this->app.keyboard()->poll();
			this->app.gamepads()->poll();
			this->gallery.update(1.0f / 60.0f);
		}

		void press(Key key)
		{
			this->app.keyboard()->on_key_down(key);
			this->tick();
			this->app.keyboard()->on_key_up(key);
			this->tick();
		}

		void focus_binding()
		{
			// Difficulty, Window, Music, Effects, Quit, then Binding. Keyboard
			// navigation focuses it without activating the row's capture action.
			for (int step = 0; step < 5; ++step) this->press(Key::down);
			REQUIRE(IsWindow(this->window));
		}

		void begin_capture()
		{
			this->press(Key::enter);
			REQUIRE(this->status_begins(L"Binding capture:"));
		}

		bool status_begins(std::wstring_view prefix) const
		{
			return this->heard.back().starts_with(prefix);
		}

		Vector2I client_at(float x, float y) const
		{
			const Vector2F resolution = this->app.resolution_manager()->resolution_vec();
			const Viewport viewport(RectangleF(Vector2F::zero, resolution));
			const Camera camera = Camera::frame(RectangleF(0.0f, 0.0f, 1280.0f, 720.0f), viewport);
			const Vector2F pixel = camera.calculate_view_position(Vector2F(x, y));
			return Vector2I(static_cast<int>(std::lround(pixel.x)), static_cast<int>(std::lround(pixel.y)));
		}

		void click(float x, float y)
		{
			this->app.mouse()->on_move(this->client_at(x, y));
			this->app.mouse()->on_button_down(MouseButton::left);
			this->tick();
			this->app.mouse()->on_button_up(MouseButton::left);
			this->tick();
		}

		void queue_binding_click()
		{
			// A complete mouse click between polls becomes a two-tick pointer
			// gesture. Cancelling or accepting capture must discard both halves.
			this->app.mouse()->on_move(this->client_at(170.0f, 500.0f));
			this->app.mouse()->on_button_down(MouseButton::left);
			this->app.mouse()->on_button_up(MouseButton::left);
		}
	};
}

TEST_CASE("gallery keyboard capture waits for its pad opener and keeps pad cancel")
{
	GalleryFixture fixture;
	fixture.focus_binding();
	GamepadState pad;
	pad.connected = true;
	fixture.source.set_state(0, pad);
	fixture.tick();
	pad.buttons = gamepad_button_bit(GamepadButton::a);
	fixture.source.set_state(0, pad);
	fixture.tick();
	REQUIRE(fixture.status_begins(L"Binding capture:"));
	const std::size_t opened = fixture.heard.size();
	fixture.press(Key::t);
	CHECK(fixture.heard.size() == opened);
	pad.buttons = 0;
	fixture.source.set_state(0, pad);
	fixture.tick();
	fixture.press(Key::u);
	REQUIRE(fixture.heard.back() == L"Binding saved: restart_previews=u");

	pad.buttons = gamepad_button_bit(GamepadButton::a);
	fixture.source.set_state(0, pad);
	fixture.tick();
	REQUIRE(fixture.status_begins(L"Binding capture:"));
	pad.buttons = gamepad_button_bit(GamepadButton::b);
	fixture.source.set_state(0, pad);
	fixture.tick();
	CHECK(fixture.heard.back() == L"Binding capture cancelled.");
	CHECK(IsWindow(fixture.window));
	pad.buttons = 0;
	fixture.source.set_state(0, pad);
	fixture.tick();
	fixture.press(Key::r);
	CHECK(fixture.heard.back() == L"Binding capture cancelled.");
	fixture.press(Key::u);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");
}

// Native null render and audio only: real state, hidden window, synthetic
// input and no Application loop or graphics/audio API device.
TEST_CASE("gallery rebinding waits for the opener and every already-held key to release")
{
	GalleryFixture fixture;
	fixture.focus_binding();
	Keyboard& keyboard = *fixture.app.keyboard();
	keyboard.on_key_down(Key::t);
	fixture.tick();
	keyboard.on_key_down(Key::enter);
	fixture.tick();
	REQUIRE(fixture.status_begins(L"Binding capture:"));
	const std::size_t opened = fixture.heard.size();

	keyboard.on_key_down(Key::u);
	fixture.tick();
	keyboard.on_key_up(Key::enter);
	fixture.tick();
	keyboard.on_key_up(Key::t);
	fixture.tick();
	CHECK(fixture.heard.size() == opened);
	CHECK(fixture.status_begins(L"Binding capture:"));
	keyboard.on_key_up(Key::u);
	fixture.tick();
	CHECK(fixture.heard.size() == opened);

	fixture.press(Key::v);
	CHECK(fixture.heard.back() == L"Binding saved: restart_previews=v");
	CHECK(IsWindow(fixture.window));
}

TEST_CASE("gallery duplicate and protected refusals preserve shortcuts and consume the committing key")
{
	GalleryFixture fixture;
	fixture.focus_binding();
	fixture.begin_capture();
	fixture.press(Key::d);
	CHECK(fixture.status_begins(L"Binding rejected: duplicate"));
	CHECK(IsWindow(fixture.window));
	fixture.press(Key::down);
	CHECK(fixture.status_begins(L"Binding rejected: protected"));
	CHECK(IsWindow(fixture.window));

	Keyboard& keyboard = *fixture.app.keyboard();
	keyboard.on_key_down(Key::t);
	fixture.tick();
	REQUIRE(fixture.heard.back() == L"Binding saved: restart_previews=t");
	const std::size_t saved = fixture.heard.size();
	fixture.tick();
	CHECK(fixture.heard.size() == saved);
	keyboard.on_key_up(Key::t);
	fixture.tick();
	CHECK(fixture.heard.size() == saved);
	fixture.press(Key::r);
	CHECK(fixture.heard.size() == saved);
	fixture.press(Key::t);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");
	CHECK(fixture.heard.size() == saved + 1);

	// The duplicate D attempt did not displace the other action. Make the
	// difficulty Hard, use its unchanged D reset, then step to Hard again.
	fixture.click(680.0f, 250.0f);
	REQUIRE(fixture.heard.back() == L"Difficulty is now Hard");
	fixture.press(Key::d);
	const std::size_t after_reset = fixture.heard.size();
	fixture.click(680.0f, 250.0f);
	CHECK(fixture.heard.back() == L"Difficulty is now Hard");
	CHECK(fixture.heard.size() == after_reset + 1);
}

TEST_CASE("gallery cancellation focus loss deactivation and reset leave a usable navigation route")
{
	GalleryFixture fixture;
	fixture.focus_binding();
	fixture.begin_capture();
	fixture.press(Key::escape);
	CHECK(fixture.heard.back() == L"Binding capture cancelled.");
	CHECK(IsWindow(fixture.window));
	fixture.press(Key::up);
	fixture.press(Key::up);
	fixture.press(Key::left);
	CHECK(fixture.heard.back() == L"Effects slider example: 90% (no audio loaded)");

	fixture.press(Key::down);
	fixture.press(Key::down);
	fixture.begin_capture();
	fixture.app.keyboard()->set_focused(false);
	fixture.tick();
	CHECK(fixture.heard.back() == L"Binding capture cancelled: focus lost.");
	CHECK(IsWindow(fixture.window));
	fixture.app.keyboard()->set_focused(true);
	fixture.tick();
	fixture.press(Key::up);
	fixture.press(Key::up);
	fixture.press(Key::right);
	CHECK(fixture.heard.back() == L"Effects slider example: 100% (no audio loaded)");

	fixture.press(Key::down);
	fixture.press(Key::down);
	fixture.begin_capture();
	static_cast<State&>(fixture.gallery).on_deactivated();
	fixture.press(Key::r);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");
	CHECK(IsWindow(fixture.window));

	fixture.begin_capture();
	fixture.press(Key::t);
	REQUIRE(fixture.heard.back() == L"Binding saved: restart_previews=t");
	fixture.press(Key::backspace);
	REQUIRE(fixture.status_begins(L"Bindings reset:"));
	const std::size_t reset = fixture.heard.size();
	fixture.press(Key::t);
	CHECK(fixture.heard.size() == reset);
	fixture.press(Key::r);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");

	// Binding is still focused: Up returns to the original Quit row, and
	// Enter remains available after every refused or cancelled capture.
	fixture.press(Key::up);
	fixture.press(Key::enter);
	CHECK_FALSE(IsWindow(fixture.window));
}

TEST_CASE("clicking another gallery row cancels capture and leaves that row usable")
{
	GalleryFixture fixture;
	fixture.focus_binding();
	fixture.begin_capture();
	fixture.click(170.0f, 250.0f);
	CHECK(fixture.status_begins(L"Binding capture cancelled"));
	CHECK(fixture.heard.back().ends_with(L"another row."));
	CHECK(IsWindow(fixture.window));
	fixture.press(Key::right);
	CHECK(fixture.heard.back() == L"Difficulty is now Hard");
	fixture.press(Key::r);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");
}

TEST_CASE("queued gallery pointer releases cannot reopen capture after Escape or a saved key")
{
	GalleryFixture fixture;
	fixture.focus_binding();
	fixture.begin_capture();
	fixture.queue_binding_click();
	fixture.app.keyboard()->on_key_down(Key::escape);
	fixture.tick();
	REQUIRE(fixture.heard.back() == L"Binding capture cancelled.");
	fixture.app.keyboard()->on_key_up(Key::escape);
	fixture.tick();
	fixture.tick();
	CHECK(fixture.heard.back() == L"Binding capture cancelled.");
	CHECK(IsWindow(fixture.window));
	fixture.press(Key::r);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");

	fixture.begin_capture();
	fixture.queue_binding_click();
	fixture.app.keyboard()->on_key_down(Key::t);
	fixture.tick();
	REQUIRE(fixture.heard.back() == L"Binding saved: restart_previews=t");
	fixture.app.keyboard()->on_key_up(Key::t);
	fixture.tick();
	fixture.tick();
	CHECK(fixture.heard.back() == L"Binding saved: restart_previews=t");
	fixture.press(Key::t);
	CHECK(fixture.heard.back() == L"Both animation previews restarted.");
	CHECK(IsWindow(fixture.window));
}

TEST_CASE("a modal opener owns a simultaneous fresh pointer press and action key")
{
	for (const bool name_mode : { false, true })
	{
		GalleryFixture fixture;
		fixture.focus_binding();
		if (name_mode) fixture.press(Key::down);
		Keyboard& keyboard = *fixture.app.keyboard();
		Mouse& mouse = *fixture.app.mouse();
		keyboard.on_key_down(Key::enter);
		keyboard.on_key_down(Key::r);
		mouse.on_move(fixture.client_at(170.0f, 250.0f));
		mouse.on_button_down(MouseButton::left);
		fixture.tick();
		INFO("name_mode=", name_mode);
		REQUIRE(fixture.status_begins(name_mode ? L"Name editing:" : L"Binding capture:"));
		const std::size_t opened = fixture.heard.size();
		keyboard.on_key_up(Key::enter);
		keyboard.on_key_up(Key::r);
		mouse.on_button_up(MouseButton::left);
		fixture.tick();
		CHECK(fixture.heard.size() == opened);
		fixture.press(Key::escape);
		CHECK(IsWindow(fixture.window));
		fixture.press(Key::r);
		CHECK(fixture.heard.back() == L"Both animation previews restarted.");
	}
}

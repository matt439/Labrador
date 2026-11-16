#include <doctest/doctest.h>

#include "samples/components/gallery_state.h"

#include "engine/app/application.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2i.h"
#include "engine/render/camera.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/viewport.h"

#include <Windows.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

using components::GalleryState;
using namespace labrador;

namespace
{
	void tick(Application& app, GalleryState& gallery)
	{
		app.mouse()->poll();
		app.touch()->poll();
		app.keyboard()->poll();
		gallery.update(1.0f / 60.0f);
	}

	void key(Application& app, GalleryState& gallery, Key value)
	{
		app.keyboard()->on_key_down(value);
		tick(app, gallery);
		app.keyboard()->on_key_up(value);
		tick(app, gallery);
	}

	Vector2I client_at(const Application& app, const Vector2F& world)
	{
		const Viewport viewport(RectangleF(Vector2F::zero,
			app.resolution_manager()->resolution_vec()));
		const Camera camera = Camera::frame(
			RectangleF(0.0f, 0.0f, 1280.0f, 720.0f), viewport);
		const Vector2F pixel = camera.calculate_view_position(world);
		return Vector2I(static_cast<int>(std::lround(pixel.x)),
			static_cast<int>(std::lround(pixel.y)));
	}

	void click(Application& app, GalleryState& gallery, const Vector2F& world)
	{
		app.mouse()->on_move(client_at(app, world));
		app.mouse()->on_button_down(MouseButton::left);
		tick(app, gallery);
		app.mouse()->on_button_up(MouseButton::left);
		tick(app, gallery);
	}
}

TEST_CASE("the gallery owns text editing, opener suppression, command precedence and font refusal")
{
	ApplicationOptions options;
	options.window_class_name = L"ComponentsTextTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	Application app(std::move(options));
	app.initialize();
	app.load_manifest("manifest.json");
	std::vector<std::wstring> heard;
	GalleryState gallery(&app,
		[&heard](const std::wstring& text) { heard.push_back(text); });
	gallery.init();
	app.mouse()->set_focused(true);
	app.touch()->set_focused(true);
	app.keyboard()->set_focused(true);
	tick(app, gallery);
	click(app, gallery, Vector2F(980.0f, 510.0f));
	REQUIRE(heard.back() == L"Name editing: Player");
	key(app, gallery, Key::escape);
	CHECK(IsWindow(static_cast<HWND>(app.window())));
	CHECK(heard.back() == L"Name kept: Player");

	// The Enter snapshot opens editing; its typed bucket cannot insert X.
	app.keyboard()->on_text(U'X');
	key(app, gallery, Key::enter);
	CHECK(heard.back() == L"Name editing: Player");
	app.keyboard()->on_key_down(Key::r);
	app.keyboard()->on_text(U'R');
	tick(app, gallery);
	CHECK(heard.back() == L"Name editing: PlayerR");
	app.keyboard()->on_key_up(Key::r);
	tick(app, gallery);
	app.keyboard()->on_text(U'\u00E9');
	tick(app, gallery);
	CHECK(heard.back() == L"Name: missing font glyph");
	key(app, gallery, Key::enter);
	CHECK(heard.back() == L"Name saved: PlayerR");

	key(app, gallery, Key::enter);
	// Backspace wins over typed text in this poll; it does not append Z.
	app.keyboard()->on_key_down(Key::backspace);
	app.keyboard()->on_text(U'Z');
	tick(app, gallery);
	CHECK(heard.back() == L"Name editing: Player");
	app.keyboard()->on_key_up(Key::backspace);
	tick(app, gallery);
	app.keyboard()->on_text(U'!');
	app.keyboard()->on_text(U'\r');
	key(app, gallery, Key::enter);
	CHECK(heard.back() == L"Name saved: Player!");

	key(app, gallery, Key::enter);
	app.keyboard()->on_text(U'?');
	tick(app, gallery);
	app.keyboard()->set_focused(false);
	tick(app, gallery);
	CHECK(heard.back() == L"Name kept: Player!");
	app.keyboard()->on_text(U'Q');
	tick(app, gallery);
	CHECK(heard.back() == L"Name kept: Player!");
	app.keyboard()->set_focused(true);
	tick(app, gallery);
	key(app, gallery, Key::enter);
	CHECK(heard.back() == L"Name editing: Player!");
	app.keyboard()->on_text(U'X');
	tick(app, gallery);
	gallery.on_deactivated();
	CHECK(heard.back() == L"Name kept: Player!");

	// A pending click on the same field cannot reopen it after a command.
	key(app, gallery, Key::enter);
	app.mouse()->on_move(client_at(app, Vector2F(980.0f, 510.0f)));
	app.mouse()->on_button_down(MouseButton::left);
	tick(app, gallery);
	app.keyboard()->on_key_down(Key::escape);
	app.mouse()->on_button_up(MouseButton::left);
	tick(app, gallery);
	CHECK(heard.back() == L"Name kept: Player!");
	app.keyboard()->on_key_up(Key::escape);
	tick(app, gallery);
	CHECK(heard.back() == L"Name kept: Player!");
	key(app, gallery, Key::enter);
	app.mouse()->on_button_down(MouseButton::left);
	tick(app, gallery);
	app.keyboard()->on_text(U'X');
	app.keyboard()->on_key_down(Key::enter);
	app.mouse()->on_button_up(MouseButton::left);
	tick(app, gallery);
	CHECK(heard.back() == L"Name saved: Player!X");
	app.keyboard()->on_key_up(Key::enter);
	tick(app, gallery);
	CHECK(heard.back() == L"Name saved: Player!X");
	CHECK_FALSE(IsWindowVisible(static_cast<HWND>(app.window())));
}

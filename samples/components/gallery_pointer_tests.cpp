#include <doctest/doctest.h>

#include "samples/components/gallery_state.h"

#include "engine/app/application.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
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
	// The gallery's layout (gallery_state.cpp), in its 1280x720 world units.
	constexpr float label_x = 160.0f;
	constexpr float value_x = 480.0f;
	constexpr float track_width = 300.0f;

	// The middle of a row's line of text, which is inside its pointer band
	// and, on a slider's row, inside its track.
	float row_middle(int row)
	{
		return 240.0f + static_cast<float>(row) * 50.0f + 10.0f;
	}

	// The client pixel the gallery's camera puts a world point in: the
	// inverse of the one GalleryState reads a pointer through.
	Vector2I client_at(const Application& app, float x, float y)
	{
		const Vector2F resolution = app.resolution_manager()->resolution_vec();
		const Viewport viewport(RectangleF(Vector2F::zero, resolution));
		const Camera camera = Camera::frame(
			RectangleF(0.0f, 0.0f, 1280.0f, 720.0f), viewport);
		const Vector2F pixel = camera.calculate_view_position(Vector2F(x, y));
		return Vector2I(static_cast<int>(std::lround(pixel.x)),
			static_cast<int>(std::lround(pixel.y)));
	}

	// One tick as the shell runs it: the devices polled, then the state.
	void tick(Application& app, GalleryState& gallery)
	{
		app.mouse()->poll();
		app.touch()->poll();
		app.keyboard()->poll();
		gallery.update(1.0f / 60.0f);
	}

	// A press and a release in the same place, a tick each.
	void click(Application& app, GalleryState& gallery, float x, float y)
	{
		Mouse& mouse = *app.mouse();
		mouse.on_move(client_at(app, x, y));
		mouse.on_button_down(MouseButton::left);
		tick(app, gallery);
		mouse.on_button_up(MouseButton::left);
		tick(app, gallery);
	}
}

// Native null render AND audio only, as the animation case beside it: the
// real state with a hidden window and synthetic mouse, touch and keyboard
// events, and no Application loop or API device. ComponentsPointerPageTests
// drives the same rules through a browser's geometry and events.
TEST_CASE("the gallery's pointer rules: clicks, value halves, drags, keys and Quit")
{
	ApplicationOptions options;
	options.window_class_name = L"ComponentsPointerTestWindow";
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
	const HWND window = static_cast<HWND>(app.window());
	REQUIRE(IsWindow(window));
	CHECK_FALSE(IsWindowVisible(window));

	// The status line's first text reaches the page as every later one does.
	REQUIRE(heard.size() == 1);
	CHECK(heard[0] ==
		L"Window mode applies; difficulty and volume sliders are examples.");

	Mouse& mouse = *app.mouse();
	Touch& touch = *app.touch();
	Keyboard& keyboard = *app.keyboard();
	mouse.set_focused(true);
	touch.set_focused(true);
	keyboard.set_focused(true);
	tick(app, gallery);

	// A click on a label only chooses the row.
	click(app, gallery, label_x + 10.0f, row_middle(0));
	CHECK(heard.size() == 1);

	// On an option row's value the right half steps on and the left half
	// back, once a click.
	click(app, gallery, value_x + 200.0f, row_middle(0));
	REQUIRE(heard.size() == 2);
	CHECK(heard.back() == L"Difficulty is now Hard");
	click(app, gallery, value_x + 8.0f, row_middle(0));
	REQUIRE(heard.size() == 3);
	CHECK(heard.back() == L"Difficulty is now Normal");

	// A press that lets go over another row completes nothing.
	mouse.on_move(client_at(app, value_x + 200.0f, row_middle(0)));
	mouse.on_button_down(MouseButton::left);
	tick(app, gallery);
	mouse.on_move(client_at(app, value_x + 200.0f, row_middle(3)));
	tick(app, gallery);
	mouse.on_button_up(MouseButton::left);
	tick(app, gallery);
	CHECK(heard.size() == 3);

	// A press on a slider's track sets it there, and a drag past its end
	// clamps; letting go reports nothing more.
	mouse.on_move(client_at(app, value_x + track_width * 0.5f, row_middle(2)));
	mouse.on_button_down(MouseButton::left);
	tick(app, gallery);
	REQUIRE(heard.size() == 4);
	CHECK(heard.back() == L"Music slider example: 50% (no audio loaded)");
	mouse.on_move(client_at(app, 1270.0f, row_middle(2)));
	tick(app, gallery);
	REQUIRE(heard.size() == 5);
	CHECK(heard.back() == L"Music slider example: 100% (no audio loaded)");
	mouse.on_button_up(MouseButton::left);
	tick(app, gallery);
	CHECK(heard.size() == 5);

	// The track is grabbed a little before its start, which is the minimum,
	// but not from further out, where the press only chooses the row.
	click(app, gallery, value_x - 10.0f, row_middle(2));
	REQUIRE(heard.size() == 6);
	CHECK(heard.back() == L"Music slider example: 0% (no audio loaded)");
	click(app, gallery, value_x - 30.0f, row_middle(2));
	CHECK(heard.size() == 6);

	// A key in the middle of a drag drops it: the slider stays where the
	// press put it however far the pointer then goes.
	mouse.on_move(client_at(app, value_x + track_width * 0.5f, row_middle(3)));
	mouse.on_button_down(MouseButton::left);
	tick(app, gallery);
	REQUIRE(heard.size() == 7);
	CHECK(heard.back() == L"Effects slider example: 50% (no audio loaded)");
	keyboard.on_key_down(Key::enter);
	mouse.on_move(client_at(app, value_x + track_width, row_middle(3)));
	tick(app, gallery);
	keyboard.on_key_up(Key::enter);
	mouse.on_move(client_at(app, value_x, row_middle(3)));
	tick(app, gallery);
	mouse.on_button_up(MouseButton::left);
	tick(app, gallery);
	CHECK(heard.size() == 7);

	// A tap on Quit's label quits, which closes the window. Last, because
	// nothing runs after it.
	const Vector2I quit = client_at(app, label_x + 10.0f, row_middle(4));
	touch.on_down(1u, quit);
	tick(app, gallery);
	CHECK(IsWindow(window));
	touch.on_up(1u, quit);
	tick(app, gallery);
	CHECK_FALSE(IsWindow(window));
	CHECK(heard.size() == 7);

	// The close posted the quit a running pump would have ended on. Nothing
	// is pumping here, so take it out of the queue for the cases after this.
	MSG message = {};
	CHECK(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) != 0);
}

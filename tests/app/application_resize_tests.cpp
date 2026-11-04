#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/renderer.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/viewport.h"
#include "engine/render/viewport_manager.h"

#include <Windows.h>

#include <stdexcept>
#include <utility>

// A live shell, including its real hidden HWND, with null render and audio.
// The native size event must update both the layout and the renderer without
// the caller setting either cache itself. No frame loop or device is needed.
namespace
{
	using namespace labrador;

	Vector2I client_size(HWND window)
	{
		RECT client = {};
		REQUIRE(GetClientRect(window, &client) != FALSE);
		return Vector2I(client.right - client.left, client.bottom - client.top);
	}

	void resize_client(HWND window, const Vector2I& requested)
	{
		const Vector2I before = client_size(window);
		RECT outer = {};
		REQUIRE(GetWindowRect(window, &outer) != FALSE);
		const int frame_width = outer.right - outer.left - before.x;
		const int frame_height = outer.bottom - outer.top - before.y;

		// Let Windows issue the real WM_SIZE. Calling set_resolution() would
		// pre-fill the layout cache and hide a missing size-event update.
		const BOOL resized = SetWindowPos(window, nullptr, 0, 0,
			requested.x + frame_width, requested.y + frame_height,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		const DWORD error = GetLastError();
		INFO("SetWindowPos error: ", error);
		REQUIRE(resized != FALSE);
		REQUIRE(client_size(window) != before);
	}

	void check_size(const Application& app, HWND window)
	{
		const Vector2I client = client_size(window);
		const Vector2F actual(static_cast<float>(client.x),
			static_cast<float>(client.y));
		const Vector2F buffer = app.renderer()->back_buffer_size();
		const Vector2F resolution = app.resolution_manager()->resolution_vec();
		const Viewport viewport = app.viewport_manager()->fullscreen_viewport();
		CAPTURE(client.x);
		CAPTURE(client.y);
		CAPTURE(buffer.x);
		CAPTURE(buffer.y);
		CAPTURE(resolution.x);
		CAPTURE(resolution.y);

		// Comparing the caches alone could let two equally stale sizes pass.
		CHECK(buffer == actual);
		CHECK(resolution == actual);
		CHECK(viewport.position() == Vector2F::zero);
		CHECK(viewport.size() == buffer);
		CHECK_FALSE(IsWindowVisible(window));
	}
}

TEST_CASE("a live Application keeps fullscreen viewport and back buffer in step after native resizing")
{
	ApplicationOptions options;
	options.window_class_name = L"ApplicationResizeTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	SUBCASE("default startup size") {}
	SUBCASE("large startup size") { options.resolution = ScreenResolution::s_3840_2160; }
	Application app(std::move(options));
	app.initialize();
	const HWND window = static_cast<HWND>(app.window());
	REQUIRE(window != nullptr);
	REQUIRE(IsWindow(window));

	// Hidden creation sends no initial size notification, and Windows can
	// clamp a startup request to the desktop's maximum tracking size. Exercise
	// the size-event contract only after a real native resize. Neither size is
	// a resolution preset: the layout must retain exact client pixels instead
	// of coercing an unknown size back to 1280x720.
	for (const Vector2I& requested : { Vector2I(400, 300), Vector2I(513, 277) })
	{
		CAPTURE(requested.x);
		CAPTURE(requested.y);
		resize_client(window, requested);
		check_size(app, window);
	}

	// Destruction closes this hidden window without queuing WM_QUIT for the
	// next case; quit() is for a running pump and is deliberately not called.
}

TEST_CASE("a live Application hands the pointer's options and calls to its window")
{
	ApplicationOptions options;
	options.window_class_name = L"ApplicationCursorTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	options.cursor_visible = false;
	options.cursor_confined = true;
	Application app(std::move(options));

	// Before the window exists a call is the option it would have been.
	app.set_cursor_confined(false);
	CHECK_FALSE(app.cursor_confined());

	app.initialize();
	const HWND window = static_cast<HWND>(app.window());
	REQUIRE(window != nullptr);
	CHECK_FALSE(app.cursor_visible());

	const HCURSOR arrow = LoadCursorW(nullptr, IDC_ARROW);
	const WPARAM over = reinterpret_cast<WPARAM>(window);
	const LPARAM client = MAKELPARAM(HTCLIENT, WM_MOUSEMOVE);

	SetCursor(arrow);
	SendMessageW(window, WM_SETCURSOR, over, client);
	CHECK(GetCursor() == nullptr);

	app.set_cursor_visible(true);
	CHECK(app.cursor_visible());
	SetCursor(nullptr);
	SendMessageW(window, WM_SETCURSOR, over, client);
	CHECK(GetCursor() == arrow);

	app.set_cursor_confined(true);
	CHECK(app.cursor_confined());
}

TEST_CASE("a live Application opens at, and resizes to, a client size no preset names")
{
	ApplicationOptions options;
	options.window_class_name = L"ApplicationClientSizeTestWindow";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	options.client_size = Vector2I(900, 500);
	Application app(std::move(options));
	app.initialize();
	const HWND window = static_cast<HWND>(app.window());
	REQUIRE(window != nullptr);

	// A hidden window makes no size report at creation, so the layout holds
	// what was asked for, and the window must have been made that size.
	CHECK(client_size(window) == Vector2I(900, 500));
	CHECK(app.resolution_manager()->resolution_ivec() == Vector2I(900, 500));
	CHECK(app.resolution_manager()->resolution() ==
		ScreenResolution::s_1280_720);

	// Small enough for any desktop CI runs on, so none of them is clamped.
	for (const Vector2I& requested :
		{ Vector2I(640, 480), Vector2I(800, 500), Vector2I(1000, 600) })
	{
		CAPTURE(requested.x);
		CAPTURE(requested.y);
		app.set_resolution(requested);
		CHECK(client_size(window) == requested);
		check_size(app, window);
		CHECK(app.resolution_manager()->resolution() ==
			ScreenResolution::s_1280_720);
	}

	SUBCASE("below the minimum is refused and changes nothing")
	{
		const Vector2I before = client_size(window);
		CHECK_THROWS_AS(app.set_resolution(Vector2I(100, 600)),
			std::invalid_argument);
		CHECK_THROWS_AS(app.set_resolution(Vector2I(800, 100)),
			std::invalid_argument);
		CHECK(client_size(window) == before);
		check_size(app, window);
	}

	SUBCASE("past the monitor comes back clamped, and the layout follows")
	{
		const Vector2I requested(30000, 20000);
		app.set_resolution(requested);
		const Vector2I got = client_size(window);
		CHECK(got.x < requested.x);
		CHECK(got.y < requested.y);
		check_size(app, window);
	}

	SUBCASE("leaving full screen comes back to the size, not the preset")
	{
		app.set_fullscreen(true);
		app.set_fullscreen(false);
		CHECK(client_size(window) == Vector2I(1000, 600));
		check_size(app, window);
	}

	SUBCASE("a preset afterwards replaces the size")
	{
		// What the preset gives on this desktop, clamped or not, is what
		// leaving full screen must come back to.
		app.set_resolution(ScreenResolution::s_1280_720);
		const Vector2I preset = client_size(window);
		CHECK(preset != Vector2I(1000, 600));
		app.set_fullscreen(true);
		app.set_fullscreen(false);
		CHECK(client_size(window) == preset);
		check_size(app, window);
	}
}

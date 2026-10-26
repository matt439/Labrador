#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/renderer.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/viewport.h"
#include "engine/render/viewport_manager.h"

#include <Windows.h>

#include <utility>

// A live shell, including its real hidden HWND, with null render and audio.
// The native size event must update both the layout and the renderer without
// the caller setting either cache itself. No frame loop or device is needed.
namespace
{
	using namespace labrador;
	using namespace mattmath;

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
		CHECK(viewport.position() == Vector2F::ZERO);
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

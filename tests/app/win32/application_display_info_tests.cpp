#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/app/win32/display_info.h"

#include <Windows.h>

TEST_CASE("a native null application forwards its real window display metadata")
{
	labrador::ApplicationOptions options;
	options.window_class_name = L"ApplicationDisplayMetadataTests";
	options.visible = false;
	options.view_capacity = 1;
	options.min_threads = 1;
	options.max_threads = 1;
	labrador::Application app(options);
	CHECK(app.display_info().monitor_id.empty());
	app.initialize();
	const labrador::WindowDisplayInfo expected = labrador::detail::query_window_display(
		static_cast<HWND>(app.window()));
	const labrador::WindowDisplayInfo actual = app.display_info();
	CHECK(actual.monitor_id == expected.monitor_id);
	CHECK(actual.nominal_refresh_hz == expected.nominal_refresh_hz);
	app.quit();
	CHECK(app.display_info().monitor_id.empty());
	CHECK_FALSE(app.display_info().nominal_refresh_hz.has_value());
	MSG quit = {};
	PeekMessageW(&quit, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE);
}

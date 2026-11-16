#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/app/window.h"

#include <Windows.h>

#include <cstddef>
#include <stdexcept>
#include <string>

using namespace labrador;

namespace
{
	class CaptionNotify final : public WindowNotify
	{
	public:
		bool fail_size = false;
		void tick() override {}
		void on_activated() override {}
		void on_deactivated() override {}
		void on_suspending() override {}
		void on_resuming() override {}
		void on_window_moved() const override {}
		void on_window_size_changed(int, int) override
		{
			if (this->fail_size)
			{
				throw std::runtime_error("icon test size failure");
			}
		}
		void on_key_down(Key) const override {}
		void on_key_up(Key) const override {}
		void on_text(char32_t) const override {}
		void on_mouse_move(int, int) const override {}
		void on_mouse_button_down(MouseButton) const override {}
		void on_mouse_button_up(MouseButton) const override {}
		void on_mouse_wheel(float) const override {}
		void on_mouse_wheel_horizontal(float) const override {}
		void on_touch_down(uint32_t, int, int) const override {}
		void on_touch_move(uint32_t, int, int) const override {}
		void on_touch_up(uint32_t, int, int) const override {}
		void on_touch_cancel(uint32_t) const override {}
	};

	WindowOptions hidden_caption_options()
	{
		WindowOptions options;
		options.window_class_name = L"LabradorTitleIconTests";
		options.window_title = L"Initial caption";
		options.client_size = Vector2I(320, 200);
		options.visible = false;
		return options;
	}

	std::wstring caption(HWND handle)
	{
		const int length = GetWindowTextLengthW(handle);
		std::wstring result(static_cast<std::size_t>(length) + 1, L'\0');
		const int copied = GetWindowTextW(handle, result.data(), length + 1);
		REQUIRE(copied == length);
		result.resize(static_cast<std::size_t>(copied));
		return result;
	}

	bool caption_class_registered()
	{
		WNDCLASSEXW info = {};
		info.cbSize = sizeof(info);
		return GetClassInfoExW(GetModuleHandleW(nullptr), L"LabradorTitleIconTests", &info) != FALSE;
	}

	HICON class_icon(HWND handle, int which = GCLP_HICON)
	{
		return reinterpret_cast<HICON>(GetClassLongPtrW(handle, which));
	}

	void check_icon_alive(HICON icon)
	{
		ICONINFO info = {};
		REQUIRE(GetIconInfo(icon, &info) != FALSE);
		// GetIconInfo gives the caller these bitmap copies; the icon remains borrowed.
		if (info.hbmColor != nullptr)
		{
			CHECK(DeleteObject(info.hbmColor) != FALSE);
		}
		if (info.hbmMask != nullptr)
		{
			CHECK(DeleteObject(info.hbmMask) != FALSE);
		}
	}

}

TEST_CASE("native title changes preserve hidden state and client size")
{
	CaptionNotify notify;
	Window window(hidden_caption_options(), &notify);
	const HWND handle = static_cast<HWND>(window.handle());
	RECT before = {};
	REQUIRE(GetClientRect(handle, &before) != FALSE);
	const std::wstring title = L"Runtime title \u03a9 \u4e2d";
	window.set_title(title);
	CHECK(caption(handle) == title);
	CHECK_FALSE(IsWindowVisible(handle));
	CHECK(GetForegroundWindow() != handle);
	RECT after = {};
	REQUIRE(GetClientRect(handle, &after) != FALSE);
	CHECK(after.right == before.right);
	CHECK(after.bottom == before.bottom);
	window.set_title(L"");
	CHECK(caption(handle).empty());
	window.set_title(L"After empty caption");
	CHECK_THROWS_AS(window.set_title(std::wstring(L"bad\0title", 9)), std::invalid_argument);
	CHECK(caption(handle) == L"After empty caption");
	window.close();
	CHECK_NOTHROW(window.set_title(std::wstring(L"ignored\0after", 13)));
	window.pump_until_quit();
	CHECK(window.exit_code() == 0);
}

TEST_CASE("native startup refuses truncated title text without retaining a class")
{
	CaptionNotify notify;
	WindowOptions options = hidden_caption_options();
	options.window_title = std::wstring(L"bad\0title", 9);
	CHECK_THROWS_AS(Window(options, &notify), std::invalid_argument);
	CHECK_FALSE(caption_class_registered());
	options.window_title = L"Retry caption";
	Window retry(options, &notify);
	CHECK(caption(static_cast<HWND>(retry.handle())) == L"Retry caption");
}

TEST_CASE("default and explicitly selected icons use shared executable resources")
{
	CaptionNotify notify;
	const HINSTANCE executable = GetModuleHandleW(nullptr);
	const HICON legacy = LoadIconW(executable, L"IDI_ICON");
	const HICON selected = LoadIconW(executable, L"WINDOW_TEST_ICON");
	REQUIRE(legacy != nullptr);
	REQUIRE(selected != nullptr);
	{
		Window window(hidden_caption_options(), &notify);
		CHECK(class_icon(static_cast<HWND>(window.handle())) == legacy);
		CHECK(class_icon(static_cast<HWND>(window.handle()), GCLP_HICONSM) == legacy);
	}
	check_icon_alive(legacy);
	WindowOptions options = hidden_caption_options();
	options.window_icon_resource = L"WINDOW_TEST_ICON";
	{
		Window window(options, &notify);
		const HWND handle = static_cast<HWND>(window.handle());
		CHECK(class_icon(handle) == selected);
		CHECK(class_icon(handle, GCLP_HICONSM) == selected);
		CHECK_THROWS_AS(Window(options, &notify), std::runtime_error);
		CHECK(IsWindow(handle) != FALSE);
		CHECK(class_icon(handle) == selected);
	}
	CHECK_FALSE(caption_class_registered());
	check_icon_alive(selected);
	Window retry(options, &notify);
	CHECK(class_icon(static_cast<HWND>(retry.handle())) == selected);
}

TEST_CASE("missing explicit icons fail by name before a native class is registered")
{
	CaptionNotify notify;
	WindowOptions options = hidden_caption_options();
	options.window_icon_resource = L"MISSING_WINDOW_TEST_ICON";
	try
	{
		Window refused(options, &notify);
		FAIL("missing explicitly requested icon was accepted");
	}
	catch (const std::runtime_error& error)
	{
		const std::string message = error.what();
		CHECK(message.find("MISSING_WINDOW_TEST_ICON") != std::string::npos);
		CHECK(message.find("executable") != std::string::npos);
	}
	CHECK_FALSE(caption_class_registered());
	options.window_icon_resource = L"";
	CHECK_THROWS_AS(Window(options, &notify), std::invalid_argument);
	options.window_icon_resource = std::wstring(L"bad\0icon", 8);
	CHECK_THROWS_AS(Window(options, &notify), std::invalid_argument);
	CHECK_FALSE(caption_class_registered());
	options.window_icon_resource.reset();
	Window retry(options, &notify);
	CHECK(retry.handle() != nullptr);
}

TEST_CASE("a later constructor failure releases the window class without destroying shared icons")
{
	CaptionNotify notify;
	notify.fail_size = true;
	WindowOptions options = hidden_caption_options();
	options.fullscreen = true;
	options.window_icon_resource = L"WINDOW_TEST_ICON";
	CHECK_THROWS_WITH_AS(Window(options, &notify), "icon test size failure", std::runtime_error);
	CHECK_FALSE(caption_class_registered());
	const HICON selected = LoadIconW(GetModuleHandleW(nullptr), L"WINDOW_TEST_ICON");
	REQUIRE(selected != nullptr);
	check_icon_alive(selected);
	notify.fail_size = false;
	Window retry(options, &notify);
	CHECK(class_icon(static_cast<HWND>(retry.handle())) == selected);
}

#if defined(LABRADOR_TEST_APP_NULL)
TEST_CASE("Application forwards icon selection and title updates before and after initialization")
{
	ApplicationOptions options;
	options.window_class_name = L"LabradorApplicationTitleTests";
	options.visible = false;
	options.view_capacity = 1;
	options.max_threads = 1;
	options.window_icon_resource = L"WINDOW_TEST_ICON";
	Application app(options);
	app.set_window_title(L"Before initialize");
	app.initialize();
	const HWND handle = static_cast<HWND>(app.window());
	REQUIRE(handle != nullptr);
	CHECK(caption(handle) == L"Before initialize");
	CHECK(class_icon(handle) == LoadIconW(GetModuleHandleW(nullptr), L"WINDOW_TEST_ICON"));
	app.set_window_title(L"After initialize");
	CHECK(caption(handle) == L"After initialize");
	CHECK_FALSE(IsWindowVisible(handle));
}
#endif

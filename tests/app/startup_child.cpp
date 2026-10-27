#include "engine/app/application.h"
#include "engine/app/window.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"

#include <Windows.h>
#include <cstdint>
#include <exception>

using labrador::ApplicationOptions;
using labrador::Key;
using labrador::MouseButton;
using labrador::Window;
using labrador::WindowNotify;
using labrador::WindowOptions;

// The child window_tests.cpp launches with a STARTUPINFO of its choosing: a
// window made as Application makes one from default ApplicationOptions, and
// an exit code saying whether the system showed it. No device, no loop, and
// nothing pumped, so the answer is what the first ShowWindow did.

namespace
{
	class SilentNotify : public WindowNotify
	{
	public:
		void tick() override {}
		void on_activated() override {}
		void on_deactivated() override {}
		void on_suspending() override {}
		void on_resuming() override {}
		void on_window_moved() const override {}
		void on_window_size_changed(int, int) override {}
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
}

// window_tests.cpp's names for the three answers.
constexpr int startup_child_visible = 10;
constexpr int startup_child_hidden = 20;
constexpr int startup_child_failed = 30;

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	try
	{
		const ApplicationOptions defaults;
		WindowOptions options;
		options.window_class_name = defaults.window_class_name;
		options.window_title = defaults.window_title;
		options.client_size = labrador::Vector2I(320, 200);
		options.fullscreen = defaults.fullscreen;
		options.visible = defaults.visible;
		options.cursor_visible = defaults.cursor_visible;
		options.cursor_confined = defaults.cursor_confined;
		options.min_window_width = defaults.min_window_width;
		options.min_window_height = defaults.min_window_height;

		SilentNotify notify;
		const Window window(options, &notify);
		return IsWindowVisible(static_cast<HWND>(window.handle())) != 0 ?
			startup_child_visible : startup_child_hidden;
	}
	catch (const std::exception&)
	{
		return startup_child_failed;
	}
}

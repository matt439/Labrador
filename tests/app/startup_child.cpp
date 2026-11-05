#include "engine/app/application.h"
#include "engine/app/window.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/math/vector2i.h"
#include "tests/app/startup_child_exit_codes.h"

#include <Windows.h>
#include <cstdint>
#include <exception>

using labrador::ApplicationOptions;
using labrador::Key;
using labrador::MouseButton;
using labrador::Window;
using labrador::WindowNotify;
using labrador::WindowOptions;

// The child startup_info_tests.cpp launches with a STARTUPINFO of its choosing:
// a window made as Application makes one from default ApplicationOptions, and
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

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	try
	{
		const WindowOptions options = labrador::window_options_for(
			ApplicationOptions(), labrador::Vector2I(320, 200));

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

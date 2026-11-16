#include "engine/app/application.h"
#include "engine/app/window.h"
#include "engine/core/state.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"
#include "engine/render/renderer.h"
#include "tests/app/startup_child_exit_codes.h"

#include <Windows.h>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string_view>

using labrador::ApplicationOptions;
using labrador::Key;
using labrador::MouseButton;
using labrador::Window;
using labrador::WindowNotify;
using labrador::WindowOptions;

// A child launched with a STARTUPINFO of the test's choosing. The direct Window
// path answers immediately; native-null also builds an Application path that
// checks hidden initialization and drawing before observing the first reveal.

namespace
{
#if defined(LABRADOR_STARTUP_APPLICATION)
	struct StartupObservation
	{
		int result = startup_child_failed;
		labrador::Vector2F first_draw_size;
	};

	class StartupState : public labrador::State
	{
	public:
		StartupState(labrador::Application* app, StartupObservation* observation,
			bool windowed_before_frame) : app_(app), observation_(observation),
			windowed_before_frame_(windowed_before_frame)
		{
		}
		void init() override
		{
			if (this->windowed_before_frame_)
			{
				this->app_->set_fullscreen(true);
				this->app_->set_fullscreen(false);
			}
		}
		void update(float) override
		{
			if (this->app_->frame_timings().frame == 0 || !this->app_->window())
			{
				return;
			}
			const HWND window = static_cast<HWND>(this->app_->window());
			if (this->app_->renderer()->back_buffer_size() !=
				this->observation_->first_draw_size)
			{
				throw std::runtime_error("Initial show resized the completed frame.");
			}
			this->observation_->result = !IsWindowVisible(window) ? startup_child_hidden :
				IsIconic(window) ? startup_child_minimized :
				IsZoomed(window) ? startup_child_maximized : startup_child_visible;
			if (IsZoomed(window))
			{
				ShowWindow(window, SW_RESTORE);
				if (this->app_->renderer()->back_buffer_size() !=
					labrador::Vector2F(320.0f, 200.0f))
				{
					throw std::runtime_error("Maximized startup lost its restore size.");
				}
			}
			this->app_->quit();
		}
		void draw(labrador::Renderer&) const override
		{
			if (this->app_->frame_timings().frame == 0)
			{
				this->observation_->first_draw_size =
					this->app_->renderer()->back_buffer_size();
			}
			if (this->app_->frame_timings().frame == 0 &&
				IsWindowVisible(static_cast<HWND>(this->app_->window())))
			{
				throw std::runtime_error("Application showed before the first frame.");
			}
		}
	private:
		labrador::Application* app_;
		StartupObservation* observation_;
		bool windowed_before_frame_;
	};
#else
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
#endif
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE,
	_In_ [[maybe_unused]] LPWSTR command_line, _In_ int)
{
	try
	{
#if defined(LABRADOR_STARTUP_APPLICATION)
		ApplicationOptions options;
		options.client_size = labrador::Vector2I(320, 200);
		options.max_threads = 1;
		options.view_capacity = 1;
		const std::wstring_view mode(command_line);
		options.visible = mode != L"hidden";
		options.fullscreen = mode == L"fullscreen-to-windowed";
		labrador::Application app(options);
		app.initialize();
		if (IsWindowVisible(static_cast<HWND>(app.window())))
		{
			return startup_child_failed;
		}
		StartupObservation observation;
		app.run(std::make_unique<StartupState>(&app, &observation,
			mode == L"fullscreen-to-windowed" || mode == L"fullscreen-round-trip"));
		return observation.result;
#else
		const WindowOptions options = labrador::window_options_for(
			ApplicationOptions(), labrador::Vector2I(320, 200));

		SilentNotify notify;
		const Window window(options, &notify);
		return IsWindowVisible(static_cast<HWND>(window.handle())) != 0 ?
			startup_child_visible : startup_child_hidden;
#endif
	}
	catch (const std::exception&)
	{
		return startup_child_failed;
	}
}

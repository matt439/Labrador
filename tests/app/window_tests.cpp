#include <doctest/doctest.h>

#include "engine/app/window.h"

#include <Windows.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using labrador::Key;
using labrador::MouseButton;
using labrador::Window;
using labrador::WindowNotify;
using labrador::WindowOptions;

// A real Win32 window, hidden. Everything here creates one with SW_HIDE, so a
// test run puts nothing on screen and steals no focus - and every assertion is
// about what the object does with the native window and the messages it
// receives, which is the half of Window that outer_size_for_client's tests
// cannot reach.
//
// The lifetime cases are docs/review/gpt6/README.md#G6-01 and the restore case
// is #G6-06. Both were probes against a hidden window before they were tests.

namespace
{
	// Records what the window told it, in order.
	class RecordingNotify : public WindowNotify
	{
	public:
		std::vector<std::string> log;

		// What tick() does, so one test can make it throw.
		std::function<void()> on_tick;

		void tick() override
		{
			this->log.push_back("tick");
			if (this->on_tick)
			{
				this->on_tick();
			}
		}

		void on_activated() override { this->log.push_back("activated"); }
		void on_deactivated() override { this->log.push_back("deactivated"); }
		void on_suspending() override { this->log.push_back("suspending"); }
		void on_resuming() override { this->log.push_back("resuming"); }
		void on_window_moved() const override {}
		void on_window_size_changed(int width, int height) override
		{
			this->log.push_back("size " + std::to_string(width) + "x" +
				std::to_string(height));
		}

		void on_key_down(Key) const override {}
		void on_key_up(Key) const override {}
		void on_text(char32_t) const override {}
		void on_mouse_move(int, int) const override {}
		void on_mouse_button_down(MouseButton) const override {}
		void on_mouse_button_up(MouseButton) const override {}
		void on_mouse_wheel(float) const override {}
		void on_mouse_wheel_horizontal(float) const override {}
	};

	// One class name for every test, deliberately. The constructor registers
	// it and the destructor unregisters it, and running these cases back to
	// back in one process is what checks that the second half happens.
	WindowOptions hidden_options()
	{
		WindowOptions options;
		options.window_class_name = L"LabradorWindowTests";
		options.window_title = L"window tests";
		options.client_size = mattmath::Vector2I(64, 64);
		return options;
	}

	bool class_is_registered()
	{
		WNDCLASSEXW info = {};
		info.cbSize = sizeof(WNDCLASSEXW);
		return GetClassInfoExW(GetModuleHandleW(nullptr),
			L"LabradorWindowTests", &info) != 0;
	}

	bool quit_is_queued()
	{
		MSG message = {};
		return PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT,
			PM_NOREMOVE) != 0;
	}
}

TEST_CASE("an ordinary close destroys the window and ends the pump")
{
	RecordingNotify notify;
	HWND handle = nullptr;
	{
		Window window(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
			&notify);
		handle = window.handle();
		REQUIRE(handle != nullptr);
		REQUIRE(IsWindow(handle));

		window.close();

		// DestroyWindow is synchronous, so by here the window has had its
		// last message and the object has stopped naming it.
		CHECK_FALSE(IsWindow(handle));
		CHECK(window.handle() == nullptr);

		// And the quit it posted is what the pump returns on.
		window.pump_until_quit();
		CHECK(window.exit_code() == 0);

		// Idempotent: nothing to destroy, nothing to post.
		window.close();
		CHECK_FALSE(quit_is_queued());
	}
	CHECK_FALSE(IsWindow(handle));
	CHECK_FALSE(class_is_registered());
}

TEST_CASE("leaving scope with the window still up destroys it")
{
	// The shape of every exit that is not the pump returning: the owner is
	// unwound while the native window still exists. This is the probe the
	// review ran - it found IsWindow true and the user data still pointing at
	// the destroyed object.
	RecordingNotify notify;
	HWND handle = nullptr;
	size_t told_before_teardown = 0;
	{
		Window window(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
			&notify);
		handle = window.handle();
		REQUIRE(IsWindow(handle));
		told_before_teardown = notify.log.size();
	}

	CHECK_FALSE(IsWindow(handle));
	CHECK_FALSE(class_is_registered());

	// Nothing DestroyWindow sent reached the owner. By the time the
	// destructor runs, in Application's case, the services those handlers
	// touch are already gone.
	CHECK(notify.log.size() == told_before_teardown);

	// And nothing was posted. The next thing the samples do on this path is
	// MessageBoxA, whose modal loop returns at once on a queued WM_QUIT.
	CHECK_FALSE(quit_is_queued());
}

TEST_CASE("a startup failure after the window is up leaves no window behind")
{
	// Application::initialize opens the window first and creates the device
	// second, and load_manifest runs after both. A throw from either unwinds
	// through here.
	RecordingNotify notify;
	HWND handle = nullptr;
	try
	{
		Window window(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
			&notify);
		handle = window.handle();
		REQUIRE(IsWindow(handle));
		throw std::runtime_error("create_device failed");
	}
	catch (const std::runtime_error&)
	{
	}

	CHECK_FALSE(IsWindow(handle));
	CHECK_FALSE(quit_is_queued());
}

TEST_CASE("an exception out of the loop leaves no window behind")
{
	// tick() is a state's update() by way of Application, and a state may
	// throw. The pump does not catch it, which is right (T6) - and then the
	// Window is destroyed with the native window still up.
	RecordingNotify notify;
	notify.on_tick = []() { throw std::runtime_error("update failed"); };

	HWND handle = nullptr;
	{
		Window window(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
			&notify);
		handle = window.handle();
		CHECK_THROWS_AS(window.pump_until_quit(), std::runtime_error);
		CHECK(IsWindow(handle));
	}

	CHECK_FALSE(IsWindow(handle));
	CHECK_FALSE(quit_is_queued());
}

TEST_CASE("the class name is free again once the window is gone")
{
	// Sequential, not simultaneous - two windows alive at once still need two
	// names, which the header says. What this pins is that one name serves
	// one window after another, which a registration nobody undid would not.
	RecordingNotify notify;
	{
		Window first(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
			&notify);
		CHECK(class_is_registered());
	}
	CHECK_FALSE(class_is_registered());
	{
		Window second(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
			&notify);
		CHECK(second.handle() != nullptr);
	}
	CHECK_FALSE(class_is_registered());
}

TEST_CASE("restoring from minimised delivers the new size, then the resume")
{
	RecordingNotify notify;
	Window window(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
		&notify);
	notify.log.clear();

	// Minimised, then restored straight into a maximised 1024x768 - which is
	// one WM_SIZE, and the first the window has seen since it went away. A
	// window restored from the taskbar into the maximised state it held
	// before, or restored after the monitor changed, arrives exactly so.
	SendMessageW(window.handle(), WM_SIZE, SIZE_MINIMIZED, 0);
	SendMessageW(window.handle(), WM_SIZE, SIZE_MAXIMIZED,
		MAKELPARAM(1024, 768));

	// The size before the resume: the resume is what reaches the state
	// stack, and a state hears the news into a shell that has finished
	// changing.
	CHECK(notify.log == std::vector<std::string>{
		"suspending", "size 1024x768", "resuming"});
}

TEST_CASE("a minimise is one suspend and a restore is one resume")
{
	// The collapsing the header describes, and the branch the restore fix
	// sits inside: a second SIZE_MINIMIZED while already minimised says
	// nothing, and neither does a restore that was never preceded by one.
	RecordingNotify notify;
	Window window(GetModuleHandleW(nullptr), SW_HIDE, hidden_options(),
		&notify);
	notify.log.clear();

	SendMessageW(window.handle(), WM_SIZE, SIZE_MINIMIZED, 0);
	SendMessageW(window.handle(), WM_SIZE, SIZE_MINIMIZED, 0);
	SendMessageW(window.handle(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(64, 64));
	SendMessageW(window.handle(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(64, 64));

	CHECK(notify.log == std::vector<std::string>{
		"suspending", "size 64x64", "resuming", "size 64x64"});
}

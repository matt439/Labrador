#include <doctest/doctest.h>

#include "engine/app/window.h"
#include "engine/app/win32/pointer_input.h"

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

// A real Win32 window, hidden. Everything here creates one with
// WindowOptions::visible false, so a test run puts nothing on screen and
// steals no focus - and every assertion is about what the object does with the
// native window and the messages it receives.
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

		mutable Key last_down = Key::none;
		mutable Key last_up = Key::none;
		mutable char32_t last_text = 0;
		mutable labrador::Mouse mouse;
		void on_key_down(Key key) const override { this->last_down = key; }
		void on_key_up(Key key) const override { this->last_up = key; }
		void on_text(char32_t text) const override { this->last_text = text; }
		void on_mouse_move(int, int) const override {}
		void on_mouse_button_down(MouseButton button) const override { this->mouse.on_button_down(button); }
		void on_mouse_button_up(MouseButton button) const override { this->mouse.on_button_up(button); }
		void on_mouse_capture_lost() const override { this->mouse.cancel_buttons(); }
		void on_mouse_wheel(float) const override {}
		void on_mouse_wheel_horizontal(float) const override {}

		mutable std::vector<std::string> touches;
		void on_touch_down(uint32_t point, int x, int y) const override
		{
			this->touches.push_back("down " + std::to_string(point) + " " +
				std::to_string(x) + "," + std::to_string(y));
		}
		void on_touch_move(uint32_t point, int x, int y) const override
		{
			this->touches.push_back("move " + std::to_string(point) + " " +
				std::to_string(x) + "," + std::to_string(y));
		}
		void on_touch_up(uint32_t point, int x, int y) const override
		{
			this->touches.push_back("up " + std::to_string(point) + " " +
				std::to_string(x) + "," + std::to_string(y));
		}
		void on_touch_cancel(uint32_t point) const override
		{
			this->touches.push_back("cancel " + std::to_string(point));
		}
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
		options.visible = false;
		return options;
	}

	// The handle as what it is on this platform. Window spells it void*,
	// because the header is the browser's too.
	HWND hwnd(const Window& window)
	{
		return static_cast<HWND>(window.handle());
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
		Window window(hidden_options(), &notify);
		handle = hwnd(window);
		REQUIRE(handle != nullptr);
		REQUIRE(IsWindow(handle));

		// WindowOptions::visible, which is how every case here stays off
		// the screen.
		CHECK_FALSE(IsWindowVisible(handle));

		window.close();

		// DestroyWindow is synchronous, so by here the window has had its
		// last message and the object has stopped naming it.
		CHECK_FALSE(IsWindow(handle));
		CHECK(hwnd(window) == nullptr);

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
		Window window(hidden_options(), &notify);
		handle = hwnd(window);
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
		Window window(hidden_options(), &notify);
		handle = hwnd(window);
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
		Window window(hidden_options(), &notify);
		handle = hwnd(window);
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
		Window first(hidden_options(), &notify);
		CHECK(class_is_registered());
	}
	CHECK_FALSE(class_is_registered());
	{
		Window second(hidden_options(), &notify);
		CHECK(hwnd(second) != nullptr);
	}
	CHECK_FALSE(class_is_registered());
}

TEST_CASE("restoring from minimised delivers the new size, then the resume")
{
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	notify.log.clear();

	// Minimised, then restored straight into a maximised 1024x768 - which is
	// one WM_SIZE, and the first the window has seen since it went away. A
	// window restored from the taskbar into the maximised state it held
	// before, or restored after the monitor changed, arrives exactly so.
	SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
	SendMessageW(hwnd(window), WM_SIZE, SIZE_MAXIMIZED,
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
	Window window(hidden_options(), &notify);
	notify.log.clear();

	SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
	SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
	SendMessageW(hwnd(window), WM_SIZE, SIZE_RESTORED, MAKELPARAM(64, 64));
	SendMessageW(hwnd(window), WM_SIZE, SIZE_RESTORED, MAKELPARAM(64, 64));

	CHECK(notify.log == std::vector<std::string>{
		"suspending", "size 64x64", "resuming", "size 64x64"});
}


TEST_CASE("keyboard messages map physical positions independently of their characters")
{
    RecordingNotify notify;
    Window window(hidden_options(), &notify);
    struct Mapping { WPARAM key; unsigned int scan; bool extended; Key expected; };
    const Mapping mappings[] = {
        { 'Z', 0x11, false, Key::w }, // AZERTY
        { 'Q', 0x1e, false, Key::a },
        { 'Y', 0x2c, false, Key::z }, // QWERTZ
        { VK_OEM_1, 0x27, false, Key::semicolon },
        { VK_HOME, 0x47, false, Key::numpad_7 }, // NumLock off
        { VK_NUMPAD7, 0x47, false, Key::numpad_7 },
        { VK_HOME, 0x47, true, Key::home },
        { VK_DELETE, 0x53, false, Key::numpad_decimal },
        { VK_DELETE, 0x53, true, Key::del },
        { VK_DIVIDE, 0x35, true, Key::numpad_divide },
        { VK_RETURN, 0x1c, true, Key::enter },
        { VK_CONTROL, 0x1d, true, Key::control },
        { VK_PAUSE, 0x45, false, Key::pause },
        { VK_NUMLOCK, 0x45, true, Key::num_lock },
        { VK_SNAPSHOT, 0x37, true, Key::print_screen },
        { 'W', 0x00, false, Key::none },
    };
    for (const Mapping& mapping : mappings)
    {
        const LPARAM position = static_cast<LPARAM>(mapping.scan << 16) |
            (mapping.extended ? (1LL << 24) : 0) | 1;
        SendMessageW(hwnd(window), WM_KEYDOWN, mapping.key, position);
        SendMessageW(hwnd(window), WM_KEYUP, mapping.key, position | (1LL << 31));
        CHECK(notify.last_down == mapping.expected);
        CHECK(notify.last_up == mapping.expected);
    }
    SendMessageW(hwnd(window), WM_SYSKEYDOWN, VK_MENU, (0x38LL << 16) | 1);
    CHECK(notify.last_down == Key::alt);
    SendMessageW(hwnd(window), WM_CHAR, L'z', 1);
    CHECK(notify.last_text == U'z');
}

TEST_CASE("capture cancellation and transfer clear buttons without a normal release")
{
    RecordingNotify notify;
    Window window(hidden_options(), &notify);
    notify.mouse.set_focused(true);
    notify.mouse.poll();
    SendMessageW(hwnd(window), WM_LBUTTONDOWN, MK_LBUTTON, 0);
    SendMessageW(hwnd(window), WM_RBUTTONDOWN, MK_LBUTTON | MK_RBUTTON, 0);
    notify.mouse.poll();
    REQUIRE(notify.mouse.held(MouseButton::left));
    REQUIRE(notify.mouse.held(MouseButton::right));
    const labrador::MouseState before = notify.mouse.state();
    SUBCASE("cancel mode")
    {
        SendMessageW(hwnd(window), WM_CANCELMODE, 0, 0);
    }
    SUBCASE("same-thread capture transfer")
    {
        WindowOptions other_options = hidden_options();
        other_options.window_class_name += L"Other";
        Window other(other_options, &notify);
        SetCapture(hwnd(other));
        REQUIRE(GetCapture() == hwnd(other));
        ReleaseCapture();
    }
    notify.mouse.poll();
    CHECK(notify.mouse.focused());
    for (const MouseButton button : { MouseButton::left, MouseButton::right })
    {
        CHECK_FALSE(notify.mouse.held(button));
        CHECK_FALSE(notify.mouse.released(button));
        CHECK_FALSE(labrador::released(notify.mouse.state(), before, button));
    }
    notify.mouse.poll();
    SendMessageW(hwnd(window), WM_LBUTTONDOWN, MK_LBUTTON, 0);
    notify.mouse.poll();
    CHECK(notify.mouse.pressed(MouseButton::left));
    SendMessageW(hwnd(window), WM_LBUTTONUP, 0, 0);
    notify.mouse.poll();
    CHECK(notify.mouse.released(MouseButton::left));
    CHECK(GetCapture() == nullptr);
}

TEST_CASE("modern power notifications are idempotent and independent of minimize")
{
    RecordingNotify notify;
    Window window(hidden_options(), &notify);
    notify.log.clear();
    SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMSUSPEND, 0);
    SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMSUSPEND, 0);
    SUBCASE("automatic resume followed by user resume")
    {
        SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, 0);
        SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMRESUMESUSPEND, 0);
        CHECK(notify.log == std::vector<std::string>{ "suspending", "resuming" });
    }
    SUBCASE("restore during power suspension stays suspended")
    {
        SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
        SendMessageW(hwnd(window), WM_SIZE, SIZE_RESTORED, MAKELPARAM(64, 64));
        CHECK(notify.log == std::vector<std::string>{ "suspending", "size 64x64" });
        SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, 0);
        CHECK(notify.log.back() == "resuming");
    }
    SUBCASE("power resume while minimized waits for restore")
    {
        SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
        SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, 0);
        SendMessageW(hwnd(window), WM_POWERBROADCAST, PBT_APMRESUMESUSPEND, 0);
        CHECK(notify.log == std::vector<std::string>{ "suspending" });
        SendMessageW(hwnd(window), WM_SIZE, SIZE_RESTORED, MAKELPARAM(64, 64));
        CHECK(notify.log == std::vector<std::string>{ "suspending", "size 64x64", "resuming" });
    }
}

TEST_CASE("a touch pointer's messages are the four touch calls")
{
	using labrador::TouchMessage;
	using labrador::touch_message;

	// The pointer's flags ride in the high word of WPARAM, its id in the low.
	const auto w = [](unsigned int flags)
	{
		return static_cast<uint64_t>(MAKEWPARAM(42, flags));
	};

	CHECK(touch_message(WM_POINTERDOWN,
		w(POINTER_MESSAGE_FLAG_INCONTACT)) == TouchMessage::down);
	CHECK(touch_message(WM_POINTERUPDATE,
		w(POINTER_MESSAGE_FLAG_INCONTACT)) == TouchMessage::move);
	CHECK(touch_message(WM_POINTERUP, w(0)) == TouchMessage::up);

	// A finger above a digitiser that reports hover is not a point.
	CHECK(touch_message(WM_POINTERUPDATE, w(POINTER_MESSAGE_FLAG_INRANGE)) ==
		TouchMessage::none);

	// Taken away rather than lifted.
	CHECK(touch_message(WM_POINTERUP, w(POINTER_MESSAGE_FLAG_CANCELED)) ==
		TouchMessage::cancel);
	CHECK(touch_message(WM_POINTERCAPTURECHANGED, w(0)) ==
		TouchMessage::cancel);

	CHECK(touch_message(WM_POINTERENTER, w(0)) == TouchMessage::none);
	CHECK(touch_message(WM_MOUSEMOVE, w(0)) == TouchMessage::none);
}

TEST_CASE("the touch signature is told from the pen's and from none")
{
	using labrador::mouse_message_is_from_touch;

	CHECK(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF515780u)));
	CHECK(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF5157FFu)));

	// A pen keeps its mouse messages: the signature without the touch bit.
	CHECK_FALSE(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF515700u)));
	CHECK_FALSE(mouse_message_is_from_touch(static_cast<intptr_t>(0xFF51577Fu)));

	// A real mouse, and anything else that happens to set the bit.
	CHECK_FALSE(mouse_message_is_from_touch(0));
	CHECK_FALSE(mouse_message_is_from_touch(static_cast<intptr_t>(0x12345680u)));
}

TEST_CASE("mouse messages Windows made from a touch are dropped, and a pen's are kept")
{
	// GetMessageExtraInfo is the thread's, and a sent message is handled on
	// this thread, so setting it here is what the window reads.
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	notify.mouse.set_focused(true);
	notify.mouse.poll();

	SetMessageExtraInfo(static_cast<LPARAM>(0xFF515780u));
	SendMessageW(hwnd(window), WM_LBUTTONDOWN, MK_LBUTTON, 0);
	notify.mouse.poll();
	CHECK_FALSE(notify.mouse.held(MouseButton::left));
	CHECK(GetCapture() == nullptr);
	SendMessageW(hwnd(window), WM_LBUTTONUP, 0, 0);

	SetMessageExtraInfo(static_cast<LPARAM>(0xFF515700u));
	SendMessageW(hwnd(window), WM_LBUTTONDOWN, MK_LBUTTON, 0);
	notify.mouse.poll();
	CHECK(notify.mouse.held(MouseButton::left));
	SendMessageW(hwnd(window), WM_LBUTTONUP, 0, 0);

	SetMessageExtraInfo(0);
	notify.mouse.poll();
	CHECK_FALSE(notify.mouse.held(MouseButton::left));
	CHECK(GetCapture() == nullptr);
}

TEST_CASE("a pointer Windows does not know as touch reaches nothing, and a lost capture cancels")
{
	// A made-up id is no kind of pointer to GetPointerType, which is the
	// question the window asks before anything reaches the touch calls -
	// and the reason the translation above is asserted on its own.
	RecordingNotify notify;
	Window window(hidden_options(), &notify);

	SendMessageW(hwnd(window), WM_POINTERDOWN,
		MAKEWPARAM(4321, POINTER_MESSAGE_FLAG_INCONTACT), MAKELPARAM(10, 10));
	SendMessageW(hwnd(window), WM_POINTERUP, MAKEWPARAM(4321, 0),
		MAKELPARAM(10, 10));
	CHECK(notify.touches.empty());

	// A lost capture is answered whatever the pointer was.
	SendMessageW(hwnd(window), WM_POINTERCAPTURECHANGED, MAKEWPARAM(4321, 0),
		0);
	CHECK(notify.touches == std::vector<std::string>{ "cancel 4321" });
}

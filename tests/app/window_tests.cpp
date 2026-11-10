#include <doctest/doctest.h>

#include "engine/app/window.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
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

namespace
{
	// How RecordingNotify logs a size, so a case can spell the entry it
	// expects.
	std::string size_entry(long width, long height)
	{
		return "size " + std::to_string(width) + "x" + std::to_string(height);
	}

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

		// What the minimised tick does and answers, when a case sets it.
		// Unset, the window gets WindowNotify's own: tick(), and no wait.
		std::function<std::chrono::nanoseconds()> on_minimized_tick;

		std::chrono::nanoseconds tick_minimized() override
		{
			if (!this->on_minimized_tick)
			{
				return WindowNotify::tick_minimized();
			}
			this->log.push_back("minimized tick");
			return this->on_minimized_tick();
		}

		void on_activated() override { this->log.push_back("activated"); }
		void on_deactivated() override { this->log.push_back("deactivated"); }
		void on_suspending() override { this->log.push_back("suspending"); }
		void on_resuming() override { this->log.push_back("resuming"); }
		void on_window_moved() const override {}
		void on_window_size_changed(int width, int height) override
		{
			this->log.push_back(size_entry(width, height));
			if (this->on_size)
			{
				this->on_size();
			}
		}

		// What a size report does after it is logged, so a case can make a
		// handler throw from inside window_proc.
		std::function<void()> on_size;

		mutable Key last_down = Key::none;
		mutable Key last_up = Key::none;
		mutable char32_t last_text = 0;
		mutable labrador::Keyboard keyboard;
		mutable labrador::Mouse mouse;
		void on_key_down(Key key) const override
		{
			this->last_down = key;
			this->keyboard.on_key_down(key);
		}
		void on_key_up(Key key) const override
		{
			this->last_up = key;
			this->keyboard.on_key_up(key);
		}
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

	// A key message's lParam: the scan code in bits 16-23, the extended-key
	// bit, and a repeat count of one. A key-up also sets the previous-state
	// and transition bits, as Windows does.
	constexpr LPARAM extended_bit = LPARAM{ 1 } << 24;
	constexpr LPARAM previous_state_bit = LPARAM{ 1 } << 30;
	constexpr LPARAM transition_bit = LPARAM{ 1 } << 31;

	LPARAM key_down_lparam(unsigned int scan, bool extended = false)
	{
		return (static_cast<LPARAM>(scan) << 16) |
			(extended ? extended_bit : 0) | 1;
	}

	LPARAM key_up_lparam(unsigned int scan, bool extended = false)
	{
		return key_down_lparam(scan, extended) | previous_state_bit |
			transition_bit;
	}

	// The messages that take focus or the whole application away, each of
	// which ends whatever input was in progress.
	struct FocusLoss { UINT message; WPARAM parameter; };
	constexpr FocusLoss focus_losses[] = {
		{ WM_KILLFOCUS, 0 },
		{ WM_ACTIVATEAPP, FALSE },
		{ WM_POWERBROADCAST, PBT_APMSUSPEND },
		{ WM_SIZE, SIZE_MINIMIZED },
	};

	// One class name for every test, deliberately. The constructor registers
	// it and the destructor unregisters it, and running these cases back to
	// back in one process is what checks that the second half happens.
	WindowOptions hidden_options()
	{
		WindowOptions options;
		options.window_class_name = L"LabradorWindowTests";
		options.window_title = L"window tests";
		options.client_size = labrador::Vector2I(64, 64);
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
	// unwound while the native window still exists. Afterwards the native
	// window and its class are gone, and nothing the teardown sent reached
	// the destroyed owner.
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

TEST_CASE("a handler that throws on a dispatched message closes the window, and the pump rethrows")
{
	// A posted WM_SIZE reaches window_proc through DispatchMessage, which is
	// user32's frame and not one an exception may cross. The window keeps it
	// and the loop ends without another tick.
	RecordingNotify notify;
	notify.on_size = []() { throw std::runtime_error("resize failed"); };

	HWND handle = nullptr;
	{
		Window window(hidden_options(), &notify);
		handle = hwnd(window);
		notify.log.clear();
		REQUIRE(PostMessageW(handle, WM_SIZE, SIZE_RESTORED,
			MAKELPARAM(80, 80)) != FALSE);

		CHECK_THROWS_WITH_AS(window.pump_until_quit(), "resize failed",
			std::runtime_error);
		CHECK(notify.log == std::vector<std::string>{ "size 80x80" });
		CHECK_FALSE(IsWindow(handle));
		CHECK(hwnd(window) == nullptr);
	}

	CHECK_FALSE(class_is_registered());
	CHECK_FALSE(quit_is_queued());
}

TEST_CASE("a handler that throws on a sent message closes the window, and the next pump rethrows")
{
	// SetWindowPos sends its WM_SIZE straight to window_proc, with no pump
	// on the stack. The call returns, the window is gone, every later call
	// does nothing, and the loop rethrows before its first tick.
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	const HWND handle = hwnd(window);
	notify.on_size = []() { throw std::runtime_error("resize failed"); };
	notify.log.clear();

	// Past the smallest window, so the size changes and Windows reports it.
	CHECK_NOTHROW(window.resize_client(labrador::Vector2I(400, 300)));
	REQUIRE(notify.log.size() == 1);
	CHECK(notify.log[0].rfind("size ", 0) == 0);
	CHECK_FALSE(IsWindow(handle));
	CHECK(hwnd(window) == nullptr);

	CHECK_THROWS_WITH_AS(window.pump_until_quit(), "resize failed",
		std::runtime_error);
	CHECK(notify.log.size() == 1);
	CHECK_FALSE(quit_is_queued());
}

TEST_CASE("a handler that throws while the window is made leaves nothing behind")
{
	// Created hidden and full screen, the window reports the size it asked
	// for before the constructor returns. A handler throwing there is the
	// constructor's exception, with no window and no class left over.
	RecordingNotify notify;
	notify.on_size = []() { throw std::runtime_error("first size refused"); };
	WindowOptions options = hidden_options();
	options.fullscreen = true;

	CHECK_THROWS_WITH_AS(Window(options, &notify), "first size refused",
		std::runtime_error);
	CHECK_FALSE(notify.log.empty());
	CHECK(FindWindowW(L"LabradorWindowTests", nullptr) == nullptr);
	CHECK_FALSE(class_is_registered());
	CHECK_FALSE(quit_is_queued());
}

TEST_CASE("every call after the window has closed does nothing")
{
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	window.close();
	window.pump_until_quit();
	notify.log.clear();

	CHECK_NOTHROW(window.resize_client(labrador::Vector2I(96, 80)));
	CHECK_NOTHROW(window.enter_fullscreen());
	CHECK_NOTHROW(window.leave_fullscreen(labrador::Vector2I(96, 80)));
	CHECK_NOTHROW(window.set_cursor_visible(false));
	CHECK_NOTHROW(window.set_cursor_confined(true));
	CHECK_NOTHROW(window.close());

	CHECK(notify.log.empty());
	CHECK(hwnd(window) == nullptr);
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
	// The collapsing the header describes: a second SIZE_MINIMIZED while
	// already minimised says nothing, and neither does a restore that was
	// never preceded by one.
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

TEST_CASE("a minimised window's loop calls the minimised tick and sleeps as long as it answers")
{
	// Minimised by the message alone, which is the state the loop reads: the
	// window stays hidden, and nothing is put on the taskbar.
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
	notify.log.clear();

	// A fifth of a second of minimised ticks, each answering 20 ms. Ten fit
	// if every wait is exact; a loop that did not wait would run this many
	// thousands of times.
	const std::chrono::steady_clock::time_point start =
		std::chrono::steady_clock::now();
	notify.on_minimized_tick = [&]()
		{
			if (std::chrono::steady_clock::now() - start >=
				std::chrono::milliseconds(200))
			{
				window.close();
			}
			return std::chrono::nanoseconds(std::chrono::milliseconds(20));
		};

	window.pump_until_quit();

	const std::ptrdiff_t minimized_ticks = std::count(notify.log.begin(),
		notify.log.end(), std::string("minimized tick"));
	const std::ptrdiff_t ticks = std::count(notify.log.begin(),
		notify.log.end(), std::string("tick"));

	// The bound is loose on purpose. A wait may end early for a message
	// the system sends any window, and the timer it sleeps on is coarse, so
	// the count is neither exact nor near its ceiling - only far from a
	// spin.
	CHECK(minimized_ticks >= 2);
	CHECK(minimized_ticks <= 60);
	CHECK(ticks == 0);
}

TEST_CASE("a message ends a minimised wait at once, and the restored loop ticks again")
{
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	const HWND handle = hwnd(window);
	SendMessageW(handle, WM_SIZE, SIZE_MINIMIZED, 0);
	notify.log.clear();

	// The owner asks for ten seconds, which the loop cuts to its own
	// longest wait of one. The restore is posted from another thread 50 ms
	// after the first minimised tick, as the system would post it, and has
	// to end the wait there.
	std::thread restorer;
	std::chrono::steady_clock::time_point start;
	notify.on_minimized_tick = [&]()
		{
			if (!restorer.joinable())
			{
				start = std::chrono::steady_clock::now();
				restorer = std::thread([handle]()
					{
						std::this_thread::sleep_for(
							std::chrono::milliseconds(50));
						std::ignore = PostMessageW(handle, WM_SIZE,
							SIZE_RESTORED, MAKELPARAM(64, 64));
					});
			}
			return std::chrono::nanoseconds(std::chrono::seconds(10));
		};
	notify.on_tick = [&]() { window.close(); };

	window.pump_until_quit();
	const std::chrono::steady_clock::duration elapsed =
		std::chrono::steady_clock::now() - start;
	REQUIRE(restorer.joinable());
	restorer.join();

	// Minimised ticks first - one, or more if the system sent the window
	// something of its own meanwhile - then the restore in the order the
	// window always gives it, then the first ordinary tick, which closes
	// the window.
	const std::vector<std::string>::const_iterator first_other =
		std::find_if(notify.log.cbegin(), notify.log.cend(),
			[](const std::string& entry) { return entry != "minimized tick"; });
	CHECK(first_other != notify.log.cbegin());
	CHECK(std::vector<std::string>(first_other, notify.log.cend()) ==
		std::vector<std::string>{ "size 64x64", "resuming", "tick" });

	// Well inside the second the wait would have run to unwoken.
	CHECK(elapsed < std::chrono::milliseconds(800));
}

TEST_CASE("an owner with no minimised tick of its own gets tick, and no wait")
{
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	SendMessageW(hwnd(window), WM_SIZE, SIZE_MINIMIZED, 0);
	notify.log.clear();

	int ticks = 0;
	notify.on_tick = [&]()
		{
			ticks++;
			if (ticks == 3)
			{
				window.close();
			}
		};

	window.pump_until_quit();
	CHECK(ticks == 3);
}

TEST_CASE("a hidden window that is not minimised never gets the minimised tick")
{
	// WindowOptions::visible false is what a smoke test and a capture tool
	// run under, and both draw: hidden is not minimised.
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	notify.log.clear();

	notify.on_minimized_tick = []()
		{
			return std::chrono::nanoseconds(std::chrono::seconds(10));
		};
	int ticks = 0;
	notify.on_tick = [&]()
		{
			ticks++;
			if (ticks == 3)
			{
				window.close();
			}
		};

	window.pump_until_quit();
	CHECK(notify.log == std::vector<std::string>{ "tick", "tick", "tick" });
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
		const LPARAM position = key_down_lparam(mapping.scan, mapping.extended);
		SendMessageW(hwnd(window), WM_KEYDOWN, mapping.key, position);
		SendMessageW(hwnd(window), WM_KEYUP, mapping.key, position | transition_bit);
		CHECK(notify.last_down == mapping.expected);
		CHECK(notify.last_up == mapping.expected);
	}
	SendMessageW(hwnd(window), WM_SYSKEYDOWN, VK_MENU, key_down_lparam(0x38));
	CHECK(notify.last_down == Key::alt);

	// F10 and Alt alone arrive as the SYS pair, and are keys like any other,
	// down then up.
	for (const Mapping& mapping : { Mapping{ VK_F10, 0x44, false, Key::f10 },
		Mapping{ VK_MENU, 0x38, false, Key::alt } })
	{
		notify.last_down = Key::none;
		notify.last_up = Key::none;
		SendMessageW(hwnd(window), WM_SYSKEYDOWN, mapping.key,
			key_down_lparam(mapping.scan));
		CHECK(notify.last_down == mapping.expected);
		CHECK(notify.last_up == Key::none);
		SendMessageW(hwnd(window), WM_SYSKEYUP, mapping.key,
			key_up_lparam(mapping.scan));
		CHECK(notify.last_up == mapping.expected);
	}

	SendMessageW(hwnd(window), WM_CHAR, L'z', 1);
	CHECK(notify.last_text == U'z');
}

TEST_CASE("UTF-16 character assembly belongs to each hidden window")
{
	RecordingNotify first_notify;
	Window first(hidden_options(), &first_notify);
	RecordingNotify second_notify;
	WindowOptions second_options = hidden_options();
	second_options.window_class_name += L"Second";
	Window second(second_options, &second_notify);

	SendMessageW(hwnd(first), WM_CHAR, 0xD83D, 1);
	CHECK(first_notify.last_text == 0);
	SendMessageW(hwnd(second), WM_CHAR, 0xDE00, 1);
	CHECK(second_notify.last_text == char32_t(0xDE00));
	SendMessageW(hwnd(first), WM_CHAR, 0xDE00, 1);
	CHECK(first_notify.last_text == char32_t(0x1F600));
}

TEST_CASE("pending UTF-16 assembly survives unrelated focus and suspension messages")
{
	for (const FocusLoss& loss : focus_losses)
	{
		RecordingNotify notify;
		Window window(hidden_options(), &notify);
		SendMessageW(hwnd(window), WM_CHAR, 0xD83D, 1);
		SendMessageW(hwnd(window), loss.message, loss.parameter, 0);
		CHECK(notify.last_text == 0);
		SendMessageW(hwnd(window), WM_CHAR, 0xDE00, 1);
		CAPTURE(loss.message);
		CHECK(notify.last_text == char32_t(0x1F600));
	}
}

TEST_CASE("modifier keys stay held until both physical sides are released")
{
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	notify.keyboard.set_focused(true);
	notify.keyboard.poll();
	notify.keyboard.poll();
	struct Modifier
	{
		WPARAM virtual_key;
		unsigned int left_scan;
		unsigned int right_scan;
		bool extended_right;
		Key key;
	};
	const Modifier modifiers[] = {
		{ VK_SHIFT, 0x2a, 0x36, false, Key::shift },
		{ VK_CONTROL, 0x1d, 0x1d, true, Key::control },
		{ VK_MENU, 0x38, 0x38, true, Key::alt },
	};
	for (const Modifier& modifier : modifiers)
	{
		for (const bool right_first : { false, true })
		{
			CAPTURE(static_cast<int>(modifier.key));
			CAPTURE(right_first);
			const auto send_key = [&](bool right, bool down)
				{
					const unsigned int scan = right
						? modifier.right_scan : modifier.left_scan;
					const bool extended = right && modifier.extended_right;
					const UINT message = modifier.key == Key::alt
						? (down ? WM_SYSKEYDOWN : WM_SYSKEYUP)
						: (down ? WM_KEYDOWN : WM_KEYUP);
					SendMessageW(hwnd(window), message, modifier.virtual_key,
						down ? key_down_lparam(scan, extended)
							: key_up_lparam(scan, extended));
					notify.keyboard.poll();
				};

			send_key(right_first, true);
			CHECK(notify.keyboard.pressed(modifier.key));
			send_key(right_first, true);
			CHECK_FALSE(notify.keyboard.pressed(modifier.key));
			send_key(!right_first, true);
			CHECK(notify.keyboard.held(modifier.key));
			CHECK_FALSE(notify.keyboard.pressed(modifier.key));
			send_key(right_first, false);
			CHECK(notify.keyboard.held(modifier.key));
			CHECK_FALSE(notify.keyboard.released(modifier.key));
			send_key(!right_first, false);
			CHECK_FALSE(notify.keyboard.held(modifier.key));
			CHECK(notify.keyboard.released(modifier.key));
			send_key(!right_first, false);
			CHECK_FALSE(notify.keyboard.released(modifier.key));
		}
	}
}

TEST_CASE("focus and suspension loss discard remembered modifier sides")
{
	for (const FocusLoss& loss : focus_losses)
	{
		RecordingNotify notify;
		Window window(hidden_options(), &notify);
		SendMessageW(hwnd(window), WM_KEYDOWN, VK_CONTROL,
			key_down_lparam(0x1d));
		SendMessageW(hwnd(window), loss.message, loss.parameter, 0);
		notify.last_up = Key::none;
		// A right-side press after the loss cannot inherit the left-side
		// hold, whose key-up went to another window while this one was away.
		SendMessageW(hwnd(window), WM_KEYDOWN, VK_CONTROL,
			key_down_lparam(0x1d, true));
		SendMessageW(hwnd(window), WM_KEYUP, VK_CONTROL,
			key_up_lparam(0x1d, true));
		CAPTURE(loss.message);
		CHECK(notify.last_up == Key::control);
	}
}

TEST_CASE("modifier sides belong to each window")
{
	RecordingNotify first_notify;
	Window first(hidden_options(), &first_notify);
	RecordingNotify second_notify;
	WindowOptions second_options = hidden_options();
	second_options.window_class_name += L"Second";
	Window second(second_options, &second_notify);
	SendMessageW(hwnd(first), WM_KEYDOWN, VK_SHIFT, key_down_lparam(0x2a));
	SendMessageW(hwnd(second), WM_KEYDOWN, VK_SHIFT, key_down_lparam(0x36));
	SendMessageW(hwnd(second), WM_KEYUP, VK_SHIFT, key_up_lparam(0x36));
	CHECK(second_notify.last_up == Key::shift);
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

TEST_CASE("a hidden pointer is no cursor over the client area and the arrow elsewhere")
{
	RecordingNotify notify;
	Window window(hidden_options(), &notify);
	const HWND handle = hwnd(window);
	const HCURSOR arrow = LoadCursorW(nullptr, IDC_ARROW);
	const WPARAM over = reinterpret_cast<WPARAM>(handle);

	window.set_cursor_visible(false);
	SetCursor(arrow);
	CHECK(SendMessageW(handle, WM_SETCURSOR, over,
		MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) == TRUE);
	CHECK(GetCursor() == nullptr);

	// The frame is the system's whatever the game asked.
	SendMessageW(handle, WM_SETCURSOR, over,
		MAKELPARAM(HTCAPTION, WM_MOUSEMOVE));
	CHECK(GetCursor() == arrow);

	window.set_cursor_visible(true);
	SetCursor(nullptr);
	SendMessageW(handle, WM_SETCURSOR, over,
		MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
	CHECK(GetCursor() == arrow);

	SUBCASE("hidden from the options, before any call")
	{
		WindowOptions options = hidden_options();
		options.window_class_name += L"Hidden";
		options.cursor_visible = false;
		Window hidden(options, &notify);
		SetCursor(arrow);
		SendMessageW(hwnd(hidden), WM_SETCURSOR,
			reinterpret_cast<WPARAM>(hwnd(hidden)),
			MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
		CHECK(GetCursor() == nullptr);
	}
}

TEST_CASE("a window nobody can see never takes the pointer, confined or not")
{
	// The clip is the whole system's pointer, so a confined window takes it
	// only while it is active and shown. A hidden one told it is active is
	// neither, and the clip in force must be whatever it was before.
	RECT before = {};
	REQUIRE(GetClipCursor(&before) != FALSE);

	RecordingNotify notify;
	WindowOptions options = hidden_options();
	options.cursor_confined = true;
	Window window(options, &notify);
	SendMessageW(hwnd(window), WM_ACTIVATEAPP, TRUE, 0);
	window.set_cursor_confined(true);

	RECT during = {};
	REQUIRE(GetClipCursor(&during) != FALSE);
	CHECK(EqualRect(&before, &during) != FALSE);

	window.set_cursor_confined(false);
	SendMessageW(hwnd(window), WM_ACTIVATEAPP, FALSE, 0);
	RECT after = {};
	REQUIRE(GetClipCursor(&after) != FALSE);
	CHECK(EqualRect(&before, &after) != FALSE);
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
	// and the reason that translation is asserted on its own, in
	// pointer_input_tests.cpp.
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

// FULL SCREEN, HIDDEN. A shown window's full-screen paths go through
// ShowWindow, which shows and activates, so they stay untested here: putting
// nothing on screen is what this file is for. These pin the hidden half.
namespace
{
	// No minimum track size, so WM_GETMINMAXINFO does not clamp the small
	// sizes these cases ask for.
	WindowOptions unclamped_hidden_options()
	{
		WindowOptions options = hidden_options();
		options.min_window_width = 0;
		options.min_window_height = 0;
		return options;
	}

	std::string last_size_report(const std::vector<std::string>& log)
	{
		for (std::vector<std::string>::const_reverse_iterator entry = log.rbegin();
			entry != log.rend(); ++entry)
		{
			if (entry->rfind("size ", 0) == 0)
			{
				return *entry;
			}
		}
		return "";
	}

	RECT monitor_of(HWND handle)
	{
		MONITORINFO info = {};
		info.cbSize = sizeof(MONITORINFO);
		REQUIRE(GetMonitorInfoW(MonitorFromWindow(handle,
			MONITOR_DEFAULTTONEAREST), &info) != 0);
		return info.rcMonitor;
	}

	RECT client_of(HWND handle)
	{
		RECT client = {};
		REQUIRE(GetClientRect(handle, &client) != FALSE);
		return client;
	}

	// Observe the activation request even when Windows leaves a hidden
	// window inactive anyway. The engine owns GWLP_USERDATA, so the probe
	// keeps its state in a separate property and forwards every message.
	class WindowPositionProbe
	{
	public:
		explicit WindowPositionProbe(HWND handle) : handle_(handle)
		{
			REQUIRE(SetPropW(handle, property_name(), this) != FALSE);
			this->previous_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
				handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(window_proc)));
			if (this->previous_ == nullptr)
			{
				RemovePropW(handle, property_name());
				FAIL("could not observe window position requests");
			}
		}

		~WindowPositionProbe()
		{
			SetWindowLongPtrW(this->handle_, GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(this->previous_));
			RemovePropW(this->handle_, property_name());
		}

		WindowPositionProbe(const WindowPositionProbe&) = delete;
		WindowPositionProbe& operator=(const WindowPositionProbe&) = delete;

		bool observed() const { return this->observed_; }
		bool no_activation() const { return this->no_activation_; }

	private:
		static const wchar_t* property_name()
		{
			return L"LabradorWindowTestsPositionProbe";
		}

		static LRESULT CALLBACK window_proc(HWND handle, UINT message,
			WPARAM w_param, LPARAM l_param)
		{
			WindowPositionProbe* probe = static_cast<WindowPositionProbe*>(
				GetPropW(handle, property_name()));
			if (message == WM_WINDOWPOSCHANGING)
			{
				const WINDOWPOS* position = reinterpret_cast<const WINDOWPOS*>(l_param);
				probe->observed_ = true;
				probe->no_activation_ &= (position->flags & SWP_NOACTIVATE) != 0;
			}
			return CallWindowProcW(probe->previous_, handle, message, w_param, l_param);
		}

		HWND handle_;
		WNDPROC previous_ = nullptr;
		bool observed_ = false;
		bool no_activation_ = true;
	};
}

TEST_CASE("resizing a hidden window never requests activation")
{
	RecordingNotify notify;
	Window window(unclamped_hidden_options(), &notify);
	const HWND handle = hwnd(window);
	SUBCASE("windowed") {}
	SUBCASE("fullscreen") { window.enter_fullscreen(); }
	notify.log.clear();
	WindowPositionProbe probe(handle);

	window.resize_client(labrador::Vector2I(400, 300));

	CHECK(probe.observed());
	CHECK(probe.no_activation());
	CHECK_FALSE(IsWindowVisible(handle));
	CHECK(GetForegroundWindow() != handle);
	CHECK(notify.log == std::vector<std::string>{ "size 400x300" });
	const RECT client = client_of(handle);
	CHECK(client.right - client.left == 400);
	CHECK(client.bottom - client.top == 300);
}

TEST_CASE("what a hidden window reports before the constructor returns")
{
	// Window's constructor promises the first size report before it returns,
	// and on Windows that report is the WM_SIZE of the first show. A window
	// created hidden is never shown, so this pins what it reports instead.
	RecordingNotify notify;

	SUBCASE("a windowed one reports nothing, and has the client size it asked for")
	{
		// Windows sends an overlapped window its first WM_SIZE when it is
		// first shown, and SW_HIDE on a window that is already hidden is not
		// a show. Nothing corrects the caller, and nothing needs to while the
		// request is above the smallest window the system allows.
		WindowOptions options = unclamped_hidden_options();
		options.client_size = labrador::Vector2I(320, 240);
		Window window(options, &notify);
		CHECK(notify.log.empty());
		const RECT client = client_of(hwnd(window));
		CHECK(client.right - client.left == 320);
		CHECK(client.bottom - client.top == 240);
	}

	SUBCASE("a full-screen one reports the size it asked for, not the monitor's")
	{
		// A popup's first WM_SIZE comes from its creation, so the report is
		// made; with no show to maximise it, it is the requested size.
		WindowOptions options = unclamped_hidden_options();
		options.client_size = labrador::Vector2I(320, 240);
		options.fullscreen = true;
		Window window(options, &notify);
		CHECK(notify.log == std::vector<std::string>{ "size 320x240" });
		CHECK_FALSE(IsWindowVisible(hwnd(window)));
	}
}

TEST_CASE("a hidden window stays hidden and inactive through enter_fullscreen")
{
	RecordingNotify notify;
	Window window(unclamped_hidden_options(), &notify);
	const HWND handle = hwnd(window);
	notify.log.clear();

	window.enter_fullscreen();

	CHECK_FALSE(IsWindowVisible(handle));
	CHECK(GetForegroundWindow() != handle);
	CHECK(notify.log.end() == std::find(notify.log.begin(), notify.log.end(),
		"activated"));
	CHECK((GetWindowLongPtrW(handle, GWL_STYLE) & WS_POPUP) != 0);
	CHECK((GetWindowLongPtrW(handle, GWL_STYLE) & WS_CAPTION) == 0);

	const RECT monitor = monitor_of(handle);
	const LONG width = monitor.right - monitor.left;
	const LONG height = monitor.bottom - monitor.top;
	CHECK(last_size_report(notify.log) == size_entry(width, height));
	const RECT client = client_of(handle);
	CHECK(client.right - client.left == width);
	CHECK(client.bottom - client.top == height);

	SUBCASE("leave_fullscreen restores the frame and the client size without showing")
	{
		notify.log.clear();
		window.leave_fullscreen(labrador::Vector2I(320, 240));

		CHECK_FALSE(IsWindowVisible(handle));
		CHECK(GetForegroundWindow() != handle);
		const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);
		CHECK((style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW);
		CHECK((style & WS_POPUP) == 0);

		const RECT restored = client_of(handle);
		CHECK(restored.right - restored.left == 320);
		CHECK(restored.bottom - restored.top == 240);
		CHECK(last_size_report(notify.log) == "size 320x240");
	}
}

TEST_CASE("a shown window stays shown through enter_fullscreen and leave_fullscreen")
{
	// Shown without activation, so the case takes no focus it does not need.
	// Full screen itself activates, as ShowWindow's maximise does.
	RecordingNotify notify;
	Window window(unclamped_hidden_options(), &notify);
	const HWND handle = hwnd(window);
	ShowWindow(handle, SW_SHOWNOACTIVATE);
	REQUIRE(IsWindowVisible(handle));

	window.enter_fullscreen();
	CHECK(IsWindowVisible(handle));
	CHECK((GetWindowLongPtrW(handle, GWL_STYLE) & WS_VISIBLE) != 0);
	CHECK((GetWindowLongPtrW(handle, GWL_STYLE) & WS_POPUP) != 0);

	notify.log.clear();
	window.leave_fullscreen(labrador::Vector2I(320, 240));
	CHECK(IsWindowVisible(handle));
	const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);
	CHECK((style & WS_VISIBLE) != 0);
	CHECK((style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW);
	CHECK((style & WS_POPUP) == 0);
	const RECT client = client_of(handle);
	CHECK(client.right - client.left == 320);
	CHECK(client.bottom - client.top == 240);
	CHECK(last_size_report(notify.log) == "size 320x240");
	ShowWindow(handle, SW_HIDE);
}

TEST_CASE("an already-windowed leave_fullscreen is a resize and nothing else")
{
	RecordingNotify notify;
	Window window(unclamped_hidden_options(), &notify);
	const HWND handle = hwnd(window);
	notify.log.clear();

	window.leave_fullscreen(labrador::Vector2I(320, 240));

	CHECK_FALSE(IsWindowVisible(handle));
	CHECK(GetForegroundWindow() != handle);
	const RECT client = client_of(handle);
	CHECK(client.right - client.left == 320);
	CHECK(client.bottom - client.top == 240);
	CHECK(notify.log == std::vector<std::string>{ "size 320x240" });
}

// PER-MONITOR AWARENESS. Each case makes its thread per-monitor aware (v2,
// or v1 where it says so) before it creates a window and puts the thread back
// after, so the rest of
// the suite runs as the samples do. A sent message does not change a window's
// real DPI, which is why these assert on outer geometry rather than on client
// pixels at a new scale; that half is an attended run on a scaled display.
namespace
{
	class PerMonitorAware
	{
	public:
		explicit PerMonitorAware(DPI_AWARENESS_CONTEXT context =
			DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) :
			previous_(SetThreadDpiAwarenessContext(context))
		{
			// NULL is the call's failure: the thread kept its old awareness,
			// and a case would run unaware while appearing to test per-monitor
			// behaviour.
			REQUIRE(this->previous_ != nullptr);
		}

		~PerMonitorAware()
		{
			SetThreadDpiAwarenessContext(this->previous_);
		}

		PerMonitorAware(const PerMonitorAware&) = delete;
		PerMonitorAware& operator=(const PerMonitorAware&) = delete;

	private:
		DPI_AWARENESS_CONTEXT previous_;
	};

	SIZE window_extent(HWND handle)
	{
		RECT rect = {};
		REQUIRE(GetWindowRect(handle, &rect) != FALSE);
		return SIZE{ rect.right - rect.left, rect.bottom - rect.top };
	}

	// What the system's own arithmetic says the window would measure at
	// `dpi`, keeping the client it has now.
	SIZE expected_extent(HWND handle, UINT dpi)
	{
		RECT rect = client_of(handle);
		REQUIRE(AdjustWindowRectExForDpi(&rect,
			static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_STYLE)), FALSE,
			static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_EXSTYLE)),
			dpi) != 0);
		return SIZE{ rect.right - rect.left, rect.bottom - rect.top };
	}

	SIZE scaled_size(HWND handle, UINT dpi, LRESULT& answered)
	{
		SIZE size = { 0, 0 };
		answered = SendMessageW(handle, WM_GETDPISCALEDSIZE, dpi,
			reinterpret_cast<LPARAM>(&size));
		return size;
	}

	WindowOptions aware_options()
	{
		WindowOptions options = unclamped_hidden_options();
		options.client_size = labrador::Vector2I(320, 240);
		return options;
	}
}

TEST_CASE("a per-monitor-aware window gets the client size it asked for")
{
	PerMonitorAware aware;
	RecordingNotify notify;
	Window window(aware_options(), &notify);
	const HWND handle = hwnd(window);
	REQUIRE(AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(handle),
		DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

	const RECT client = client_of(handle);
	CHECK(client.right - client.left == 320);
	CHECK(client.bottom - client.top == 240);

	window.resize_client(labrador::Vector2I(400, 300));
	const RECT resized = client_of(handle);
	CHECK(resized.right - resized.left == 400);
	CHECK(resized.bottom - resized.top == 300);
}

TEST_CASE("WM_GETDPISCALEDSIZE answers the outer size that keeps the client")
{
	PerMonitorAware aware;
	RecordingNotify notify;
	Window window(aware_options(), &notify);
	const HWND handle = hwnd(window);

	for (const UINT dpi : { 120u, 144u, 192u })
	{
		CAPTURE(dpi);
		LRESULT answered = FALSE;
		const SIZE size = scaled_size(handle, dpi, answered);
		const SIZE expected = expected_extent(handle, dpi);
		CHECK(answered == TRUE);
		CHECK(size.cx == expected.cx);
		CHECK(size.cy == expected.cy);
	}

	// The frame grows with the DPI, so the answers are not the current size.
	LRESULT answered = FALSE;
	CHECK(scaled_size(handle, 192, answered).cy > window_extent(handle).cy);
}

TEST_CASE("WM_DPICHANGED applies the suggested rect")
{
	PerMonitorAware aware;
	RecordingNotify notify;
	Window window(aware_options(), &notify);
	const HWND handle = hwnd(window);
	notify.log.clear();

	const RECT suggested = { 40, 50, 40 + 500, 50 + 400 };
	SendMessageW(handle, WM_DPICHANGED, MAKEWPARAM(144, 144),
		reinterpret_cast<LPARAM>(&suggested));

	RECT now = {};
	GetWindowRect(handle, &now);
	CHECK(now.left == suggested.left);
	CHECK(now.top == suggested.top);
	CHECK(now.right == suggested.right);
	CHECK(now.bottom == suggested.bottom);

	const RECT client = client_of(handle);
	CHECK(notify.log == std::vector<std::string>{
		size_entry(client.right - client.left, client.bottom - client.top) });
	CHECK_FALSE(IsWindowVisible(handle));
}

TEST_CASE("a full-screen window answers its current size to a DPI change")
{
	// WS_POPUP has no frame, so keeping the client is keeping the window: a
	// monitor-sized window stays monitor-sized rather than being scaled.
	PerMonitorAware aware;
	RecordingNotify notify;
	Window window(aware_options(), &notify);
	const HWND handle = hwnd(window);
	window.enter_fullscreen();

	const SIZE current = window_extent(handle);
	for (const UINT dpi : { 120u, 144u, 192u })
	{
		CAPTURE(dpi);
		LRESULT answered = FALSE;
		const SIZE size = scaled_size(handle, dpi, answered);
		CHECK(answered == TRUE);
		CHECK(size.cx == current.cx);
		CHECK(size.cy == current.cy);
	}
}

TEST_CASE("a per-monitor v1 window keeps its client pixels through a DPI change")
{
	// v1 is never asked first and its frame is not scaled, so the rect it is
	// sent keeps its logical size; applying it would rescale the client.
	PerMonitorAware aware(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
	RecordingNotify notify;
	Window window(aware_options(), &notify);
	const HWND handle = hwnd(window);
	REQUIRE(AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(handle),
		DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE));

	const RECT client = client_of(handle);
	CHECK(client.right - client.left == 320);
	CHECK(client.bottom - client.top == 240);

	RECT before = {};
	GetWindowRect(handle, &before);
	notify.log.clear();

	const RECT suggested = { 40, 50, 40 + 500, 50 + 400 };
	SendMessageW(handle, WM_DPICHANGED, MAKEWPARAM(144, 144),
		reinterpret_cast<LPARAM>(&suggested));

	RECT now = {};
	GetWindowRect(handle, &now);
	CHECK(now.left == before.left);
	CHECK(now.top == before.top);
	CHECK(now.right == before.right);
	CHECK(now.bottom == before.bottom);
	CHECK(notify.log.empty());

	window.resize_client(labrador::Vector2I(400, 300));
	const RECT resized = client_of(handle);
	CHECK(resized.right - resized.left == 400);
	CHECK(resized.bottom - resized.top == 300);
}

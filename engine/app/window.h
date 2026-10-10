#pragma once

#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/math/vector2i.h"

#include <memory>
#include <string>

namespace labrador
{
	// What a window has to tell whoever owns it. Modelled on DeviceNotify
	// (engine/render/renderer.h) and const-qualified the same way: a handler
	// that only reads the world is const, and one that changes it is not.
	//
	// THE ACTIVATION FOUR ARE ON THE CHANGING SIDE, because they carry the
	// news into the state stack, where a game's own code runs and may push,
	// pop or transition. Fanning that out from a const handler would compile
	// - the states hang off unique_ptrs, whose constness is one level deep -
	// which is the reason the signature says so rather than leaning on it.
	class WindowNotify
	{
	public:
		// The frame. It is not a window event - it is what the loop does
		// when there is nothing else to do. On Windows that is an empty
		// queue, and WM_PAINT needs it too: while the user drags an edge
		// Windows owns the loop, and painting is the only way back in. In a
		// browser it is the animation-frame callback, which is the only loop
		// there is.
		virtual void tick() = 0;

		virtual void on_activated() = 0;
		virtual void on_deactivated() = 0;
		virtual void on_suspending() = 0;
		virtual void on_resuming() = 0;
		virtual void on_window_moved() const = 0;
		virtual void on_window_size_changed(int width, int height) = 0;

		// THE KEYBOARD AND THE MOUSE, and they are here rather than behind a
		// reader in engine/input/ because there is nowhere else they could be.
		// A pad is polled: input/xinput/ asks XInput for a snapshot and owes
		// this file nothing. These two arrive as messages in this window's
		// queue, or as events on its canvas, so the only way into the input
		// module is out through here - which is the whole reason `input` is
		// fed rather than read, and the reason nothing in it names a window
		// (keyboard.h says it at length).
		//
		// const, like on_window_moved and for the same reason: they change
		// nothing about the window, and everything they do reach is borrowed and
		// fed rather than asked anything.
		//
		// Already translated, both directions. `Key` and `MouseButton` are the
		// engine's own names, decided by the window's implementation from the
		// platform's codes, and `codepoint` is UTF-32 with any surrogate pair
		// already assembled. Nothing above this line meets a VK_ constant, a
		// KeyboardEvent.code string or a UTF-16 unit - message translation
		// lives in the window, and that is the whole of the job this class
		// exists to hand over.
		virtual void on_key_down(Key key) const = 0;
		virtual void on_key_up(Key key) const = 0;
		virtual void on_text(char32_t codepoint) const = 0;

		virtual void on_mouse_move(int x, int y) const = 0;
		virtual void on_mouse_button_down(MouseButton button) const = 0;
		virtual void on_mouse_button_up(MouseButton button) const = 0;
		// Cancel any held gesture without treating capture transfer as release.
		virtual void on_mouse_capture_lost() const {}

		// Notches, signed, fractional on a high-resolution wheel. Two
		// functions rather than one with an axis flag, because a caller
		// reading `on_mouse_wheel(delta, true)` cannot tell which way `true`
		// points without looking it up (T4).
		virtual void on_mouse_wheel(float notches) const = 0;
		virtual void on_mouse_wheel_horizontal(float notches) const = 0;

	protected:
		~WindowNotify() = default;
	};

	// What the window needs to exist. The names are ApplicationOptions' names
	// on purpose: this is the subset of them a window can act on, and nothing
	// about their meaning changes on the way across.
	struct WindowOptions
	{
		// Unique per process, so a game that ever opens two windows needs two.
		// A browser has neither a window class nor a title of the game's - the
		// page owns both - so neither is read there.
		std::wstring window_class_name = L"LabradorWindowClass";
		std::wstring window_title = L"Labrador";

		// CLIENT pixels - the area the game draws into, not the outer rect.
		// Turning one into the other is this class's job and nobody else's.
		mattmath::Vector2I client_size;

		bool fullscreen = false;

		// Whether the window is shown at all. A hidden window still has a
		// client area, a device still draws into it and its messages still
		// arrive, which is what a smoke test or a capture tool wants: a whole
		// shell that puts nothing on screen and takes no focus. A browser
		// shows whatever the page shows, so it is not read there.
		bool visible = true;

		int min_window_width = 320;
		int min_window_height = 200;
	};

	// The platform's window: a Win32 window on Windows (engine/app/win32/) and
	// the page's canvas in a browser (engine/app/web/). Exactly one is
	// compiled, chosen by the platform being built for rather than by a cache
	// variable, because neither half can be built for the other's platform.
	//
	// THIS HEADER IS NEUTRAL AND THE PLATFORM IS BEHIND Impl, which is the
	// shape GamepadReader and ThreadPool have and for a reason that is not
	// taste: application.h includes this file, and check_engine_includes.cmake
	// fails the build for any file outside a platform folder that includes a
	// header inside one. A window.h in app/win32/ would make application.h a
	// platform header. What the pointer costs is one hop on a path that runs
	// once per message.
	//
	// WHAT IS DELIBERATELY NOT HERE: the handlers themselves. They stay on
	// Application and arrive through WindowNotify, because message translation
	// lives here and what a message means does not.
	//
	// Nothing forces it. No handler here ends below the seam, so a Window
	// taking these messages would not have to include a backend - which makes
	// this a judgement about layering and nothing else: the better reason to
	// keep them where they are, and a worse one to move them.
	class Window
	{
	public:
		// Creates the window, or takes the page's canvas, and shows it.
		// Throws std::runtime_error naming the step that failed (T6).
		//
		// `notify` is taken here rather than set afterwards, and that is a
		// contract rather than a preference: the first size report arrives
		// before this constructor returns, and it is load-bearing - it is what
		// corrects the caller to the client size the window really got. On
		// Windows that is the WM_SIZE ShowWindow fires, and it matters most
		// going full screen at launch, where the monitor decides the size and
		// nothing here knows it. In a browser it is the canvas, measured here,
		// because the page laid it out and nothing here chose its size. A
		// notify set after construction would miss it, and the owner would
		// never learn the size it got.
		Window(const WindowOptions& options, WindowNotify* notify);

		// Takes down what the constructor made, on every path out - not only
		// the one where the loop ended.
		//
		// THE LOOP IS NOT THE ONLY WAY OUT of the scope that owns a Window.
		// create_device throwing, a manifest that does not open, a state's
		// update() throwing out of tick(): each unwinds the owner while the
		// window still exists, and a window left behind would go on calling
		// into an object that is gone.
		//
		// `notify` is never called from in here. Whatever the platform says
		// while the window is taken down arrives after the owner has started
		// destroying itself - in Application's case with the renderer and the
		// input devices already gone - so none of it is forwarded, and nothing
		// is left queued that would end a message loop run afterwards.
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		// The native window, as Renderer::create_device takes it: an HWND on
		// Windows, and in a browser the canvas's event target as Emscripten's
		// html5 functions name it, which is a C string.
		//
		// Null once the window is gone, whether close() took it or the user
		// did. Nothing that reaches a stale handle is a valid call, and a
		// caller holding one after run() has returned would be making one.
		void* handle() const;

		// The process exit code, valid once pump_until_quit has returned.
		int exit_code() const;

		// Runs the frame loop until the window closes, calling notify->tick()
		// once per frame.
		//
		// On Windows: one message if one is waiting, and otherwise tick().
		// Ticking only on an empty queue is the loop, not an implementation
		// detail of it - draining the queue first is a defensible design and
		// a different one, and it changes frame pacing. An exception out of
		// tick() or out of a handler comes out of this call.
		//
		// IN A BROWSER THIS CALL DOES NOT RETURN, and the stack below it does
		// not survive it. The loop is the browser's: tick() runs once per
		// animation frame, after control has gone back to the page, and
		// Emscripten gets there by throwing through every frame between here
		// and main - which runs their destructors before the first tick. So
		// nothing tick() reaches may live on that stack; Application::run says
		// what that means for a game. And with no caller left to throw to, an
		// exception out of tick() or out of a handler ends the loop the way
		// close() does and is reported instead: to stderr, and to the page's
		// Module.onError if it supplied one.
		void pump_until_quit();

		// Ends the loop. A game asking to exit does not need to know which
		// platform it is on, so this is the whole of the quit path.
		//
		// On Windows it destroys the window, which ends the pump. In a browser
		// it cancels the frame loop, stops listening to the canvas and calls
		// the page's Module.onQuit if it supplied one, because the page is
		// what decides what a finished game looks like. A second call, or a
		// call after the window has already gone, does nothing.
		void close() const;

		// Resizes so `client_size` pixels are left to draw into, under
		// whatever frame the window is currently wearing, and reports what it
		// actually got - a size past the monitor's comes back clamped.
		//
		// In a browser the page lays the canvas out and the drawing buffer
		// follows that box, so there is nothing to ask for. The report is
		// still made, of the size the canvas already has, because it is the
		// report that corrects the caller (Application::set_resolution).
		void resize_client(const mattmath::Vector2I& client_size) const;

		// Borderless and monitor-sized, and back again at `client_size`.
		//
		// In a browser, the Fullscreen API on the canvas. A browser grants it
		// only inside an input event, so a request made from a frame waits
		// for the next key or button event and is made there.
		void enter_fullscreen() const;
		void leave_fullscreen(const mattmath::Vector2I& client_size) const;

	private:
		// Declared here and defined once per platform, so the platform's
		// types stay in the translation unit that calls them. The platform
		// holds Impl's address for the window's whole life - as the window's
		// user data on Windows, as every html5 callback's in a browser - which
		// is why a Window is neither copied nor moved.
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}

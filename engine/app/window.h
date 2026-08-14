#pragma once

#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/math/vector2i.h"

#include <Windows.h>
#include <string>

namespace artattack
{
	// What a window has to tell whoever owns it. Modelled on DeviceNotify
	// (engine/render/renderer.h) and const-qualified to match, because these
	// are the handlers Application already had: five of them read the world
	// and two change it, and copying that distinction wrong makes Application
	// abstract.
	class WindowNotify
	{
	public:
		// The queue is empty, so this is the frame. It is not a window event -
		// it is what the pump does when there is nothing else to do, and it is
		// here because WM_PAINT needs it too: while the user drags an edge
		// Windows owns the loop, and painting is the only way back in.
		virtual void tick() = 0;

		virtual void on_activated() const = 0;
		virtual void on_deactivated() const = 0;
		virtual void on_suspending() const = 0;
		virtual void on_resuming() = 0;
		virtual void on_window_moved() const = 0;
		virtual void on_display_change() const = 0;
		virtual void on_window_size_changed(int width, int height) = 0;

		// THE KEYBOARD AND THE MOUSE, and they are here rather than behind a
		// reader in engine/input/ because there is nowhere else they could be.
		// A pad is polled: input/xinput/ asks XInput for a snapshot and owes
		// this file nothing. These two arrive as messages in this window's
		// queue, so the only way into the input module is out through here -
		// which is the whole reason `input` is fed rather than read, and the
		// reason nothing in it names a window (keyboard.h says it at length).
		//
		// const, like the five above and for the same reason: they change
		// nothing about the window and everything they do reach is borrowed.
		//
		// Already translated, both directions. `Key` and `MouseButton` are the
		// engine's own names, decided in window.cpp from the platform's codes,
		// and `codepoint` is UTF-32 with any surrogate pair already assembled.
		// Nothing above this line meets a VK_ constant or a UTF-16 unit -
		// message translation lives here, and that is the whole of the job
		// this class exists to hand over.
		virtual void on_key_down(Key key) const = 0;
		virtual void on_key_up(Key key) const = 0;
		virtual void on_text(char32_t codepoint) const = 0;

		virtual void on_mouse_move(int x, int y) const = 0;
		virtual void on_mouse_button_down(MouseButton button) const = 0;
		virtual void on_mouse_button_up(MouseButton button) const = 0;

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
		std::wstring window_class_name = L"ArtAttackWindowClass";
		std::wstring window_title = L"ArtAttack";

		// CLIENT pixels - the area the game draws into, not the outer rect.
		// Turning one into the other is this class's job and nobody else's.
		mattmath::Vector2I client_size;

		bool fullscreen = false;

		int min_window_width = 320;
		int min_window_height = 200;
	};

	// The Win32 window, and the only file in the engine that knows the game
	// runs on Windows at all outside the render and input backends.
	//
	// ARCHITECTURE says platform code lives in the backend subfolders. This is
	// the third case, named there rather than smuggled: `app` is already the
	// module allowed to depend on everything, PHILOSOPHY already lists
	// windowing alongside the rendering backend as platform code at the edge,
	// and a second platform moves this pair down a folder without renaming the
	// class or touching a call site.
	//
	// WHAT IS DELIBERATELY NOT HERE: the seven handlers themselves. They stay
	// on Application and arrive through WindowNotify, and one of them is the
	// reason. on_display_change ends in DeviceResources::UpdateColorSpace, so
	// a Window that handled its own messages would be a third file outside
	// engine/render/d3d11/ including the backend, which ARCHITECTURE forbids
	// in as many words. Message translation lives here; what a message means
	// does not.
	class Window
	{
	public:
		// Registers the class, creates the window and shows it. Throws
		// std::runtime_error naming the step that failed (T6).
		//
		// `notify` is taken here rather than set afterwards, and that is a
		// contract rather than a preference: WM_CREATE and the WM_SIZE that
		// ShowWindow fires both arrive before this constructor returns, and
		// that WM_SIZE is load-bearing - it is what corrects the caller to the
		// client size the window really got, which matters most going full
		// screen at launch, where the monitor decides the size and nothing
		// here knows it. A setter-based design drops those two messages on the
		// floor and silently reinstates the bug.
		Window(HINSTANCE instance, int show_command,
			const WindowOptions& options, WindowNotify* notify);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		HWND handle() const;

		// The process exit code, valid once pump_until_quit has returned.
		int exit_code() const;

		// Runs the message loop until WM_QUIT: one message if one is waiting,
		// and otherwise notify->tick(). Ticking only on an empty queue is the
		// loop, not an implementation detail of it - draining the queue first
		// is a defensible design and a different one, and it changes frame
		// pacing.
		void pump_until_quit();

		// Destroys the window, which ends the pump. The whole of the quit
		// path: a game asking to exit does not need to know it is on Win32.
		void close() const;

		// Resizes so `client_size` pixels are left to draw into, under
		// whatever frame the window is currently wearing.
		void resize_client(const mattmath::Vector2I& client_size) const;

		// Borderless and monitor-sized, and back again at `client_size`.
		void enter_fullscreen() const;
		void leave_fullscreen(const mattmath::Vector2I& client_size) const;

		// The outer window size that leaves `client_size` pixels to draw into
		// under `style`/`ex_style`. Every Win32 call that sizes a window takes
		// the outer rect and every resolution this engine is asked for is
		// client area, so this conversion sits between the two - without it a
		// windowed 1280x720 delivered about 1264x681, silently, at every
		// preset.
		//
		// Public and static because it is the one piece of this class that can
		// be tested without an HINSTANCE and a message pump.
		static mattmath::Vector2I outer_size_for_client(
			const mattmath::Vector2I& client_size, DWORD style, DWORD ex_style);

	private:
		// The same, for the style the window is wearing right now.
		mattmath::Vector2I outer_size_for_client(
			const mattmath::Vector2I& client_size) const;

		static LRESULT CALLBACK window_proc(HWND window, UINT message,
			WPARAM w_param, LPARAM l_param);

		// DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE, for the reason
		// the constructor gives: messages arrive while it is still running, so
		// everything window_proc reads has to be initialised before handle_ is
		// assigned. These were function-local statics in window_proc, which
		// worked only because there is one window per process - and which
		// zero-initialised before main, so the safety was free and accidental.
		WindowNotify* notify_ = nullptr;
		bool in_sizemove_ = false;
		bool in_suspend_ = false;
		bool minimized_ = false;
		int min_width_ = 0;
		int min_height_ = 0;
		int exit_code_ = 0;

		// How many mouse buttons are down, and it exists to balance SetCapture
		// against ReleaseCapture.
		//
		// Without capture, WM_MOUSEMOVE stops the instant the cursor crosses
		// the client edge - so a slider dragged too far, or a marquee pulled
		// past the corner, freezes where it left and then jumps when the
		// cursor comes back. Capture is what makes a drag one gesture.
		//
		// It is a COUNT and not a flag because capture is per window, not per
		// button. Pressing left, then right, then releasing left would release
		// the capture with the right button still held if this were a bool,
		// and the drag would break in the middle for no reason the player
		// could see. Capture is taken when the count leaves zero and released
		// when it returns.
		int held_buttons_ = 0;

		// The high half of a surrogate pair, waiting for its low half.
		//
		// WM_CHAR carries one UTF-16 code unit, so anything past the basic
		// plane - an emoji, most of the CJK extensions - arrives as two
		// messages that mean one character. Assembling them is message
		// translation and therefore this file's job: engine/input/keyboard.h
		// takes a char32_t and never learns that Windows speaks UTF-16.
		//
		// Zero when nothing is pending, which no real high surrogate is.
		wchar_t pending_high_surrogate_ = 0;

		HWND handle_ = nullptr;
	};
}

#include "engine/app/window.h"

#include <Windows.h>

#include <memory>
#include <stdexcept>
#include <tuple>

namespace labrador
{
	namespace
	{
		// Set-1 scan positions preserve physical bindings across keyboard layouts.
		// The extended bit distinguishes the navigation cluster from the numpad;
		// WM_CHAR remains the layout-aware text channel. Unknown positions drop.
		Key key_from_message(WPARAM w_param, LPARAM l_param)
		{
			// Pause and NumLock share scan 0x45; Windows identifies the E1 Pause
			// sequence through its virtual key rather than the E0 extended bit.
			if (w_param == VK_PAUSE) { return Key::pause; }
			if (w_param == VK_NUMLOCK) { return Key::num_lock; }
			const unsigned int scan = (static_cast<unsigned long long>(l_param) >> 16) & 0xffu;
			const bool extended = (l_param & (1LL << 24)) != 0;
			if (extended)
			{
				switch (scan)
				{
				case 0x1c: return Key::enter;
				case 0x1d: return Key::control;
				case 0x35: return Key::numpad_divide;
				case 0x37: return Key::print_screen;
				case 0x38: return Key::alt;
				case 0x47: return Key::home;
				case 0x48: return Key::up;
				case 0x49: return Key::page_up;
				case 0x4b: return Key::left;
				case 0x4d: return Key::right;
				case 0x4f: return Key::end;
				case 0x50: return Key::down;
				case 0x51: return Key::page_down;
				case 0x52: return Key::insert;
				case 0x53: return Key::del;
				default: return Key::none;
				}
			}
			switch (scan)
			{
			case 0x01: return Key::escape;
			case 0x02: return Key::digit_1;
			case 0x03: return Key::digit_2;
			case 0x04: return Key::digit_3;
			case 0x05: return Key::digit_4;
			case 0x06: return Key::digit_5;
			case 0x07: return Key::digit_6;
			case 0x08: return Key::digit_7;
			case 0x09: return Key::digit_8;
			case 0x0a: return Key::digit_9;
			case 0x0b: return Key::digit_0;
			case 0x0c: return Key::minus;
			case 0x0d: return Key::equals;
			case 0x0e: return Key::backspace;
			case 0x0f: return Key::tab;
			case 0x10: return Key::q;
			case 0x11: return Key::w;
			case 0x12: return Key::e;
			case 0x13: return Key::r;
			case 0x14: return Key::t;
			case 0x15: return Key::y;
			case 0x16: return Key::u;
			case 0x17: return Key::i;
			case 0x18: return Key::o;
			case 0x19: return Key::p;
			case 0x1a: return Key::left_bracket;
			case 0x1b: return Key::right_bracket;
			case 0x1c: return Key::enter;
			case 0x1d: return Key::control;
			case 0x1e: return Key::a;
			case 0x1f: return Key::s;
			case 0x20: return Key::d;
			case 0x21: return Key::f;
			case 0x22: return Key::g;
			case 0x23: return Key::h;
			case 0x24: return Key::j;
			case 0x25: return Key::k;
			case 0x26: return Key::l;
			case 0x27: return Key::semicolon;
			case 0x28: return Key::apostrophe;
			case 0x29: return Key::grave;
			case 0x2a: return Key::shift;
			case 0x2b: return Key::backslash;
			case 0x2c: return Key::z;
			case 0x2d: return Key::x;
			case 0x2e: return Key::c;
			case 0x2f: return Key::v;
			case 0x30: return Key::b;
			case 0x31: return Key::n;
			case 0x32: return Key::m;
			case 0x33: return Key::comma;
			case 0x34: return Key::period;
			case 0x35: return Key::slash;
			case 0x36: return Key::shift;
			case 0x37: return Key::numpad_multiply;
			case 0x38: return Key::alt;
			case 0x39: return Key::space;
			case 0x3a: return Key::caps_lock;
			case 0x3b: return Key::f1;
			case 0x3c: return Key::f2;
			case 0x3d: return Key::f3;
			case 0x3e: return Key::f4;
			case 0x3f: return Key::f5;
			case 0x40: return Key::f6;
			case 0x41: return Key::f7;
			case 0x42: return Key::f8;
			case 0x43: return Key::f9;
			case 0x44: return Key::f10;
			case 0x45: return Key::num_lock;
			case 0x46: return Key::scroll_lock;
			case 0x47: return Key::numpad_7;
			case 0x48: return Key::numpad_8;
			case 0x49: return Key::numpad_9;
			case 0x4a: return Key::numpad_subtract;
			case 0x4b: return Key::numpad_4;
			case 0x4c: return Key::numpad_5;
			case 0x4d: return Key::numpad_6;
			case 0x4e: return Key::numpad_add;
			case 0x4f: return Key::numpad_1;
			case 0x50: return Key::numpad_2;
			case 0x51: return Key::numpad_3;
			case 0x52: return Key::numpad_0;
			case 0x53: return Key::numpad_decimal;
			case 0x57: return Key::f11;
			case 0x58: return Key::f12;
			default: return Key::none;
			}
		}

		// The thumb buttons ride in the high word. There is no ceiling on how
		// many a mouse may report, so anything past the second answers
		// MouseButton::none and is dropped.
		MouseButton x_button(WPARAM w_param)
		{
			const WORD which = HIWORD(w_param);
			if (which == XBUTTON1)
			{
				return MouseButton::x1;
			}
			if (which == XBUTTON2)
			{
				return MouseButton::x2;
			}
			return MouseButton::none;
		}

		// SIGNED, and the cast is the whole point. The coordinates ride in
		// lParam as two words, and while a button is captured the cursor can
		// be left of or above the client area - which is a negative number
		// that reads as roughly 65,000 if the word is taken as unsigned.
		int low_word_signed(LPARAM l_param)
		{
			return static_cast<int>(static_cast<short>(LOWORD(l_param)));
		}

		int high_word_signed(LPARAM l_param)
		{
			return static_cast<int>(static_cast<short>(HIWORD(l_param)));
		}

		// The outer window size that leaves `client_size` pixels to draw into
		// under `style`/`ex_style`. Every Win32 call that sizes a window takes
		// the outer rect and every resolution this engine is asked for is
		// client area, so this conversion sits between the two - without it a
		// windowed 1280x720 would deliver about 1264x681, silently, at every
		// preset.
		mattmath::Vector2I outer_size_for_client(
			const mattmath::Vector2I& client_size, DWORD style, DWORD ex_style)
		{
			RECT rect = { 0, 0, static_cast<LONG>(client_size.x),
				static_cast<LONG>(client_size.y) };

			// FALSE: no menu bar. This engine's window never has one, and a menu
			// would change the answer by its height.
			if (AdjustWindowRectEx(&rect, style, FALSE, ex_style) == 0)
			{
				// Nothing to fall back to but the request. A style this call cannot
				// account for costs the game its frame's worth of pixels, which is
				// where it started.
				return client_size;
			}

			return mattmath::Vector2I(static_cast<int>(rect.right - rect.left),
				static_cast<int>(rect.bottom - rect.top));
		}
	}

	// The Win32 window. Everything a message needs to find is here, and the
	// window's user data points at it from WM_CREATE to WM_NCDESTROY.
	struct Window::Impl
	{
		Impl(const WindowOptions& options, WindowNotify* window_notify);
		~Impl();

		Impl(const Impl&) = delete;
		Impl& operator=(const Impl&) = delete;

		void update_suspension();

		// outer_size_for_client, for the style the window is wearing right now.
		mattmath::Vector2I outer_size(
			const mattmath::Vector2I& client_size) const;

		static LRESULT CALLBACK window_proc(HWND window, UINT message,
			WPARAM w_param, LPARAM l_param);

		// DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE, for the reason
		// the constructor gives: messages arrive while it is still running, so
		// everything window_proc reads has to be initialised before handle is
		// assigned.
		WindowNotify* notify = nullptr;
		bool in_sizemove = false;
		bool in_suspend = false;
		bool minimized = false;
		bool power_suspended = false;
		int min_width = 0;
		int min_height = 0;
		int exit_code = 0;

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
		int held_buttons = 0;

		// The high half of a surrogate pair, waiting for its low half.
		//
		// WM_CHAR carries one UTF-16 code unit, so anything past the basic
		// plane - an emoji, most of the CJK extensions - arrives as two
		// messages that mean one character. Assembling them is message
		// translation and therefore this file's job: engine/input/keyboard.h
		// takes a char32_t and never learns that Windows speaks UTF-16.
		//
		// Zero when nothing is pending, which no real high surrogate is.
		wchar_t pending_high_surrogate = 0;

		// What the destructor needs to unregister the class, kept because the
		// class is registered in the constructor and a registration outlives
		// the window it was made for: a second Window with the same name in
		// the same process is a failed RegisterClassExW otherwise.
		//
		// The executable's own module, which is what wWinMain is handed and
		// what the window class and the icon resource are looked up in.
		HINSTANCE instance = GetModuleHandleW(nullptr);
		std::wstring class_name;

		// Null before CreateWindowExW returns and null again from WM_NCDESTROY
		// on, which is the last message a window receives. The destructor
		// reads it to decide whether there is anything left to destroy.
		HWND handle = nullptr;
	};

	Window::Impl::Impl(const WindowOptions& options,
		WindowNotify* window_notify) :
		notify(window_notify),
		min_width(options.min_window_width),
		min_height(options.min_window_height),
		class_name(options.window_class_name)
	{
		WNDCLASSEXW window_class = {};
		window_class.cbSize = sizeof(WNDCLASSEXW);
		window_class.style = CS_HREDRAW | CS_VREDRAW;
		window_class.lpfnWndProc = window_proc;
		window_class.hInstance = this->instance;
		window_class.hIcon = LoadIconW(this->instance, L"IDI_ICON");
		window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		window_class.lpszClassName = options.window_class_name.c_str();
		window_class.hIconSm = LoadIconW(this->instance, L"IDI_ICON");

		if (RegisterClassExW(&window_class) == 0)
		{
			throw std::runtime_error("Could not register the window class.");
		}

		const DWORD style = static_cast<DWORD>(
			options.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW);
		const DWORD ex_style = static_cast<DWORD>(
			options.fullscreen ? WS_EX_TOPMOST : 0);

		// THE REQUESTED SIZE IS CLIENT AREA. CreateWindowExW takes an OUTER
		// rect, so handing it the requested resolution straight spends the
		// caption and the borders out of the game's own pixels: a windowed
		// 1280x720 came out as roughly 1264x681 to draw into, at every preset,
		// and nothing said so. AdjustWindowRectEx is the only way to ask what
		// frame this style costs, and it answers zero for the WS_POPUP branch,
		// which is why one call covers both.
		const mattmath::Vector2I outer =
			outer_size_for_client(options.client_size, style, ex_style);

		// This Impl rides in as the create parameter and is stashed in the
		// window's user data by WM_CREATE, so window_proc can find it without
		// a global. Every member window_proc reads is already initialised -
		// see the declaration order above.
		this->handle = CreateWindowExW(ex_style,
			options.window_class_name.c_str(),
			options.window_title.c_str(), style,
			CW_USEDEFAULT, CW_USEDEFAULT, outer.x, outer.y,
			nullptr, nullptr, this->instance, this);

		if (this->handle == nullptr)
		{
			// The destructor does not run for an object whose constructor
			// threw, so the registration above is this line's to undo. Left
			// registered, the caller's retry - or the next test in the same
			// process - fails one step earlier, with a message about the class
			// rather than about whatever was actually wrong.
			UnregisterClassW(this->class_name.c_str(), this->instance);
			throw std::runtime_error("Could not create the window.");
		}

		// AND THE WM_SIZE THIS PRODUCES IS WORTH SOMETHING. It arrives before
		// the device exists, so the renderer half of on_window_size_changed
		// still does nothing - DeviceResources::WindowSizeChanged returns early
		// with no window set. The resolution-manager half does not, so by the
		// time the caller reads its resolution back for create_device, that is
		// the client size the window really got. It matters most for the
		// full-screen branch, where SW_SHOWMAXIMIZED decides the size and
		// nothing here knows it. Without that read-back the swap chain is
		// created at the saved preset and then stretched non-uniformly to the
		// monitor (DXGI_SCALING_STRETCH), which is the form of this a shipped
		// sample hits.
		//
		// SW_SHOWDEFAULT is what wWinMain's own show command would have said:
		// it asks for whatever the process that started this one put in its
		// STARTUPINFO, which is where that argument comes from.
		ShowWindow(this->handle,
			!options.visible ? SW_HIDE :
			options.fullscreen ? SW_SHOWMAXIMIZED : SW_SHOWDEFAULT);
	}

	Window::Impl::~Impl()
	{
		// Null on the ordinary exit: the pump returned on WM_QUIT, which came
		// from WM_DESTROY, and WM_NCDESTROY below cleared it on the way out.
		// Non-null is every other exit - an initialisation step that threw
		// after the window was up, a manifest that would not open, an
		// exception out of the loop - and on those the native window is still
		// there with its user data pointing at this object.
		if (this->handle != nullptr)
		{
			// DETACH FIRST. DestroyWindow sends WM_DESTROY and WM_NCDESTROY
			// synchronously, and whatever else the window's state earns it -
			// a focused window is told it lost focus, an active one that it
			// was deactivated. Every one of those forwards through notify,
			// whose owner is mid-destruction: Application destroys its members
			// in reverse declaration order, so the keyboard and the renderer
			// those handlers reach are already gone. With the user data at
			// zero, window_proc's `self` is null for all of them and they fall
			// through to DefWindowProc.
			//
			// It also means WM_DESTROY does not post WM_QUIT, which is the
			// point rather than a side effect. Nobody is pumping, and the next
			// thing the samples do on this path is MessageBoxA, which runs its
			// own modal loop - and that loop treats a queued WM_QUIT as an
			// instruction to return at once, before the player has read the
			// error the box exists to show.
			SetWindowLongPtr(this->handle, GWLP_USERDATA, 0);
			DestroyWindow(this->handle);
			this->handle = nullptr;
		}

		// After the window, because it refuses while one of the class exists.
		// The return value is not checked: this is teardown, and T6 says it
		// stays silent.
		UnregisterClassW(this->class_name.c_str(), this->instance);
	}

	Window::Window(const WindowOptions& options, WindowNotify* notify) :
		impl_(std::make_unique<Impl>(options, notify))
	{
	}

	Window::~Window() = default;

	void* Window::handle() const
	{
		return this->impl_->handle;
	}

	int Window::exit_code() const
	{
		return this->impl_->exit_code;
	}

	void Window::pump_until_quit()
	{
		MSG message = {};
		while (message.message != WM_QUIT)
		{
			if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
			else
			{
				this->impl_->notify->tick();
			}
		}
		this->impl_->exit_code = static_cast<int>(message.wParam);
	}

	void Window::close() const
	{
		if (this->impl_->handle != nullptr)
		{
			DestroyWindow(this->impl_->handle);
		}
	}

	void Window::resize_client(const mattmath::Vector2I& client_size) const
	{
		// Adjusted for whatever frame the window is currently wearing, so the
		// caller gets the client area it asked for rather than that minus a
		// caption. The WM_SIZE this produces reports what the window actually
		// became, which is not always what was asked for - a size past the
		// monitor's comes back clamped.
		const mattmath::Vector2I outer = this->impl_->outer_size(client_size);
		SetWindowPos(this->impl_->handle, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER);
	}

	void Window::enter_fullscreen() const
	{
		const HWND handle = this->impl_->handle;
		SetWindowLongPtr(handle, GWL_STYLE, WS_POPUP);
		SetWindowLongPtr(handle, GWL_EXSTYLE, WS_EX_TOPMOST);
		SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowWindow(handle, SW_SHOWMAXIMIZED);
	}

	void Window::leave_fullscreen(const mattmath::Vector2I& client_size) const
	{
		const HWND handle = this->impl_->handle;

		// The style is restored BEFORE the frame arithmetic below, so that
		// arithmetic reads the ordinary window's frame and not WS_POPUP's
		// nothing.
		SetWindowLongPtr(handle, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowLongPtr(handle, GWL_EXSTYLE, 0);

		const mattmath::Vector2I outer = this->impl_->outer_size(client_size);

		ShowWindow(handle, SW_SHOWNORMAL);
		SetWindowPos(handle, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	mattmath::Vector2I Window::Impl::outer_size(
		const mattmath::Vector2I& client_size) const
	{
		return outer_size_for_client(client_size,
			static_cast<DWORD>(GetWindowLongPtrW(this->handle, GWL_STYLE)),
			static_cast<DWORD>(GetWindowLongPtrW(this->handle, GWL_EXSTYLE)));
	}

	void Window::Impl::update_suspension()
	{
		const bool suspended = this->minimized || this->power_suspended;
		if (suspended == this->in_suspend) { return; }
		this->in_suspend = suspended;
		if (suspended) { this->notify->on_suspending(); }
		else { this->notify->on_resuming(); }
	}

	// Every message either forwards through WindowNotify or is Windows
	// housekeeping. There is nothing game-specific here, which is the reason
	// it is in the engine and not copied into every project's main.cpp.
	LRESULT CALLBACK Window::Impl::window_proc(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		auto* self = reinterpret_cast<Impl*>(
			GetWindowLongPtr(window, GWLP_USERDATA));

		switch (message)
		{
		case WM_CREATE:
			if (l_param)
			{
				auto params = reinterpret_cast<LPCREATESTRUCTW>(l_param);
				SetWindowLongPtr(window, GWLP_USERDATA,
					reinterpret_cast<LONG_PTR>(params->lpCreateParams));
			}
			break;

		case WM_PAINT:
			// While the user drags the window Windows owns the loop, so the only
			// way to keep drawing is from inside the paint message.
			if (self && self->in_sizemove)
			{
				self->notify->tick();
			}
			else
			{
				PAINTSTRUCT paint;
				std::ignore = BeginPaint(window, &paint);
				EndPaint(window, &paint);
			}
			break;

		case WM_MOVE:
			// GATED ON !in_sizemove, THE SAME WAY WM_SIZE BELOW IS, and the
			// two have to be gated together. Dragging the LEFT or TOP edge
			// moves the origin as well as the size, so Windows sends WM_MOVE
			// per step of the drag - and this handler asks the renderer for a
			// size, which is exactly the term the drag is allowed to be out of
			// date about. Two of the backends answer it from a swap chain and
			// so answer the size they were last told, making the call a
			// self-comparison that changes nothing; the GL backend answers it
			// from the window (renderer.h, back_buffer_size), so ungated, every
			// step hands it the already-current client rect. That costs a
			// viewport, a clear and a per-view reset per mouse-move step, and
			// then has gl answer "nothing changed" to the WM_EXITSIZEMOVE
			// below - the one message that ends a resize, which the other
			// backends answer true to.
			if (self && !self->in_sizemove)
			{
				self->notify->on_window_moved();
			}
			break;

		case WM_SIZE:
			if (self && w_param == SIZE_MINIMIZED)
			{
				self->minimized = true;
				self->update_suspension();
			}
			else if (self && self->minimized)
			{
				self->minimized = false;

				// THE SIZE GOES OUT TOO, and before the resume. This branch
				// used to end at on_resuming() and swallow the dimensions the
				// message carries, on the assumption that a window comes back
				// the size it left - and a window restored from the taskbar
				// into the maximised state it held before, or restored after
				// the monitor changed under it, does not. Both arrive as one
				// WM_SIZE, the first this branch has seen since the minimise,
				// so what it does not deliver is never delivered: the layout
				// and the back buffer stayed at the pre-minimise size while
				// the window drew at the new one.
				//
				// Size first because of what the two mean to the owner. The
				// size re-points the layout and resizes the swap chain; the
				// resume is the one that reaches the state stack, and
				// application.cpp's rule for that is that a state hears the
				// news last of all, into a shell that has already finished
				// becoming what the news describes.
				//
				// Gated on in_sizemove exactly as the ordinary branch below
				// is, for the reason given at WM_MOVE.
				if (!self->in_sizemove)
				{
					self->notify->on_window_size_changed(
						LOWORD(l_param), HIWORD(l_param));
				}
				self->update_suspension();
			}
			else if (self && !self->in_sizemove)
			{
				self->notify->on_window_size_changed(
					LOWORD(l_param), HIWORD(l_param));
			}
			break;

		case WM_ENTERSIZEMOVE:
			if (self)
			{
				self->in_sizemove = true;
			}
			break;

		case WM_EXITSIZEMOVE:
			if (self)
			{
				self->in_sizemove = false;

				RECT client;
				GetClientRect(window, &client);
				self->notify->on_window_size_changed(
					client.right - client.left, client.bottom - client.top);
			}
			break;

		case WM_GETMINMAXINFO:
			if (l_param && self)
			{
				auto info = reinterpret_cast<MINMAXINFO*>(l_param);
				info->ptMinTrackSize.x = self->min_width;
				info->ptMinTrackSize.y = self->min_height;
			}
			break;

		case WM_ACTIVATEAPP:
			if (self)
			{
				if (w_param)
				{
					self->notify->on_activated();
				}
				else
				{
					self->notify->on_deactivated();
				}
			}
			break;

		case WM_POWERBROADCAST:
			if (self)
			{
				switch (w_param)
				{
				case PBT_APMSUSPEND:
					self->power_suspended = true;
					self->update_suspension();
					return TRUE;
				case PBT_APMRESUMEAUTOMATIC:
				case PBT_APMRESUMESUSPEND:
					self->power_suspended = false;
					self->update_suspension();
					return TRUE;
				default:
					break;
				}
			}
			break;

		// THE SYS VARIANTS ARE HERE TOO, and then fall through to
		// DefWindowProc rather than being swallowed. Alt is a key like any
		// other and a game may bind it, but Alt+F4 and F10 are the system's
		// and stay the system's - handling the message is not the same as
		// consuming it.
		//
		// Key repeat needs no filtering. Windows resends WM_KEYDOWN while a
		// key is held; Keyboard::on_key_down sets a bit that is already set,
		// and because the edges are derived from two polled frames rather than
		// from these messages, a repeat cannot manufacture a second press.
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (self)
			{
				self->notify->on_key_down(key_from_message(w_param, l_param));
			}
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (self)
			{
				self->notify->on_key_up(key_from_message(w_param, l_param));
			}
			break;

		// TYPED TEXT, which is the one channel polling cannot rebuild - the
		// shift resolution, the key repeat, the dead keys and the IME have all
		// already happened by the time a character arrives here.
		//
		// WM_SYSCHAR is deliberately NOT handled. It is what Alt+F produces,
		// and treating it as text types an "f" into whatever field is open
		// every time a player reaches for a menu.
		case WM_CHAR:
			if (self)
			{
				const wchar_t unit = static_cast<wchar_t>(w_param);

				// A high surrogate is half a character. Hold it and wait; the
				// low half is the very next message.
				if (unit >= 0xD800 && unit <= 0xDBFF)
				{
					self->pending_high_surrogate = unit;
					break;
				}

				char32_t codepoint = static_cast<char32_t>(unit);

				if (unit >= 0xDC00 && unit <= 0xDFFF &&
					self->pending_high_surrogate != 0)
				{
					codepoint = 0x10000u +
						((static_cast<char32_t>(
							self->pending_high_surrogate) - 0xD800u) << 10) +
						(static_cast<char32_t>(unit) - 0xDC00u);
				}

				// Cleared whatever happened, including the case where a low
				// half never came: a stale high surrogate joined to the next
				// unrelated character would corrupt a good one as well as the
				// lost one. An unpaired half passed on as-is becomes U+FFFD
				// inside Keyboard::on_text, which is where that rule lives.
				self->pending_high_surrogate = 0;

				self->notify->on_text(codepoint);
			}
			break;

		case WM_MOUSEMOVE:
			if (self)
			{
				self->notify->on_mouse_move(low_word_signed(l_param),
					high_word_signed(l_param));
			}
			break;

		// NO DOUBLE-CLICK MESSAGES ARRIVE, and that is deliberate rather than
		// missing. The window class above is registered without CS_DBLCLKS, so
		// Windows never substitutes WM_LBUTTONDBLCLK for the second press of a
		// pair - every press is an ordinary button-down and none is lost. What
		// counts as a double click is a policy the game owns, and two presses
		// with their timing is what it needs to decide.
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
			if (self)
			{
				const MouseButton button =
					message == WM_LBUTTONDOWN ? MouseButton::left :
					message == WM_RBUTTONDOWN ? MouseButton::right :
					message == WM_MBUTTONDOWN ? MouseButton::middle :
					x_button(w_param);

				if (button != MouseButton::none)
				{
					// Capture on the way out of zero. See held_buttons above
					// for why this counts rather than flags.
					if (self->held_buttons == 0)
					{
						SetCapture(window);
					}
					++self->held_buttons;

					self->notify->on_mouse_button_down(button);
				}
			}
			// The X button messages are documented as returning TRUE; the
			// other three fall through to DefWindowProc as usual.
			if (message == WM_XBUTTONDOWN)
			{
				return TRUE;
			}
			break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
			if (self)
			{
				const MouseButton button =
					message == WM_LBUTTONUP ? MouseButton::left :
					message == WM_RBUTTONUP ? MouseButton::right :
					message == WM_MBUTTONUP ? MouseButton::middle :
					x_button(w_param);

				if (button != MouseButton::none)
				{
					if (self->held_buttons > 0)
					{
						--self->held_buttons;
						if (self->held_buttons == 0)
						{
							ReleaseCapture();
						}
					}

					self->notify->on_mouse_button_up(button);
				}
			}
			if (message == WM_XBUTTONUP)
			{
				return TRUE;
			}
			break;

		case WM_CAPTURECHANGED:
			// Normal last-button release has already reduced the count to zero.
			// Transfer/cancellation can happen while the application keeps focus.
			if (self && self->held_buttons > 0)
			{
				self->held_buttons = 0;
				self->notify->on_mouse_capture_lost();
			}
			break;

		case WM_MOUSEWHEEL:
			if (self)
			{
				self->notify->on_mouse_wheel(
					static_cast<float>(
						static_cast<short>(HIWORD(w_param))) /
					static_cast<float>(WHEEL_DELTA));
			}
			break;

		case WM_MOUSEHWHEEL:
			if (self)
			{
				self->notify->on_mouse_wheel_horizontal(
					static_cast<float>(
						static_cast<short>(HIWORD(w_param))) /
					static_cast<float>(WHEEL_DELTA));
			}
			break;

		// ONLY WHILE OWNED. The user data is zero for a window the destructor
		// is taking down, and that destructor says why the quit must not be
		// posted then. For close() and for the user's own X it is set, and
		// this is the quit the pump is waiting for.
		case WM_DESTROY:
			if (self)
			{
				PostQuitMessage(0);
			}
			break;

		// The last message a window receives. The handle is invalid the
		// moment this returns, so the object stops naming it here - which is
		// what lets the destructor tell an ordinary exit from an unwind, and
		// makes a second close() a no-op rather than a call on a stale HWND.
		case WM_NCDESTROY:
			if (self)
			{
				self->handle = nullptr;
				SetWindowLongPtr(window, GWLP_USERDATA, 0);
			}
			break;

		case WM_MENUCHAR:
			// A menu is active and the key pressed matches no mnemonic. Swallow it
			// so Windows does not beep.
			return MAKELRESULT(0, MNC_CLOSE);

		default:
			break;
		}

		return DefWindowProc(window, message, w_param, l_param);
	}
}

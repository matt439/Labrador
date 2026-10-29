#include "engine/app/window.h"
#include "engine/app/win32/keyboard_input.h"
#include "engine/app/win32/pointer_input.h"
#include "engine/app/win32/system_command.h"

#include <Windows.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace labrador
{
	namespace
	{

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

		// Every mouse message Windows may synthesise from a touch: the ones
		// window_proc drops when mouse_message_is_from_touch says so.
		bool is_mouse_input(UINT message)
		{
			switch (message)
			{
			case WM_MOUSEMOVE:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
				return true;
			default:
				return false;
			}
		}

		// The outer window size that leaves `client_size` pixels to draw into
		// under `style`/`ex_style` at `dpi`. Every Win32 call that sizes a
		// window takes the outer rect and every resolution this engine is
		// asked for is client area, so this conversion sits between the two -
		// without it a windowed 1280x720 would deliver about 1264x681,
		// silently, at every preset.
		//
		// The DPI is frame_dpi's, below.
		Vector2I outer_size_for_client(
			const Vector2I& client_size, DWORD style, DWORD ex_style,
			UINT dpi)
		{
			RECT rect = { 0, 0, static_cast<LONG>(client_size.x),
				static_cast<LONG>(client_size.y) };

			// FALSE: no menu bar. This engine's window never has one, and a menu
			// would change the answer by its height.
			if (AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, dpi) == 0)
			{
				// Nothing to fall back to but the request. A style this call cannot
				// account for costs the game its frame's worth of pixels, which is
				// where it started.
				return client_size;
			}

			return Vector2I(static_cast<int>(rect.right - rect.left),
				static_cast<int>(rect.bottom - rect.top));
		}

		// Whether the window is per-monitor aware v2, the one awareness under
		// which the system draws its frame at the monitor's scale and asks
		// before resizing it for a DPI change.
		bool is_per_monitor_v2(HWND window)
		{
			return AreDpiAwarenessContextsEqual(
				GetWindowDpiAwarenessContext(window),
				DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
		}

		// THE DPI THE FRAME IS DRAWN AT. Under per-monitor awareness v2 that is
		// the window's own, the scale of the monitor it is on, so a frame
		// measured at any other scale leaves the client off the size asked
		// for. Under every other awareness, per-monitor v1 included, the
		// system does not scale the frame and it is drawn at the system DPI,
		// which is also what an unaware or system-aware window reports as its
		// own.
		UINT frame_dpi(HWND window)
		{
			return is_per_monitor_v2(window)
				? GetDpiForWindow(window) : GetDpiForSystem();
		}

		// The rect of the monitor the window is on, or nearest to, in screen
		// pixels: what a borderless full-screen window covers. A window that is
		// shown is maximised to it by ShowWindow; one that is hidden is sized to
		// it here, because ShowWindow cannot size without showing.
		RECT monitor_rect(HWND handle)
		{
			MONITORINFO info = {};
			info.cbSize = sizeof(MONITORINFO);
			if (GetMonitorInfoW(MonitorFromWindow(handle,
				MONITOR_DEFAULTTONEAREST), &info) == 0)
			{
				throw std::runtime_error(
					"Could not read the size of the window's monitor.");
			}
			return info.rcMonitor;
		}

		// WM_PAINT with nothing to draw: validating the update region is what
		// stops Windows sending the message again.
		LRESULT paint_nothing(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param)
		{
			PAINTSTRUCT paint;
			std::ignore = BeginPaint(window, &paint);
			EndPaint(window, &paint);
			return DefWindowProc(window, message, w_param, l_param);
		}

		// The X button messages are documented as returning TRUE; the other
		// mouse messages go to DefWindowProc as usual.
		LRESULT mouse_default(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param)
		{
			if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP)
			{
				return TRUE;
			}
			return DefWindowProc(window, message, w_param, l_param);
		}

		// WM_GETDPISCALEDSIZE: the outer size at the new DPI that keeps the
		// client's pixels. window_proc gives the reason.
		LRESULT dpi_scaled_size(HWND window, WPARAM w_param, LPARAM l_param)
		{
			if (!l_param || IsIconic(window))
			{
				return FALSE;
			}

			RECT client;
			GetClientRect(window, &client);
			const Vector2I outer = outer_size_for_client(
				Vector2I(client.right - client.left,
					client.bottom - client.top),
				static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)),
				static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)),
				LOWORD(w_param));
			SIZE* const size = reinterpret_cast<SIZE*>(l_param);
			size->cx = outer.x;
			size->cy = outer.y;
			return TRUE;
		}

		// WM_DPICHANGED: apply the suggested rect under per-monitor v2 only.
		// window_proc gives the reason.
		LRESULT dpi_changed(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param)
		{
			if (!is_per_monitor_v2(window))
			{
				return DefWindowProc(window, message, w_param, l_param);
			}
			if (l_param)
			{
				const RECT* const suggested = reinterpret_cast<const RECT*>(l_param);
				SetWindowPos(window, nullptr, suggested->left, suggested->top,
					suggested->right - suggested->left,
					suggested->bottom - suggested->top,
					SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return 0;
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
		bool update_modifier(Key key, LPARAM l_param, bool down);

		// Clips the pointer to the client area when it should be confined
		// now, and releases a clip this window made when it should not
		// (Window::set_cursor_confined).
		void update_cursor_clip(HWND window);

		// Shows the pointer's current visibility at once if it is over the
		// client area, rather than at the next WM_SETCURSOR.
		void apply_cursor_shape(HWND window) const;

		// outer_size_for_client, for the style the window is wearing right now.
		Vector2I outer_size(
			const Vector2I& client_size) const;

		static LRESULT CALLBACK window_proc(HWND window, UINT message,
			WPARAM w_param, LPARAM l_param);

		// window_proc's owned-window messages, one group each.
		LRESULT on_size_message(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param);
		LRESULT on_activation_message(HWND window, UINT message,
			WPARAM w_param, LPARAM l_param);
		LRESULT on_key_message(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param);
		LRESULT on_char_message(HWND window, WPARAM w_param, LPARAM l_param);
		LRESULT on_mouse_message(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param);
		LRESULT on_cursor_message(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param);
		LRESULT on_pointer_message(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param);
		LRESULT on_destroy_message(HWND window, UINT message, WPARAM w_param,
			LPARAM l_param);

		// DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE, for the reason
		// the constructor gives: messages arrive while it is still running, so
		// everything window_proc reads has to be initialised before handle is
		// assigned.
		WindowNotify* notify = nullptr;
		bool in_sizemove = false;
		bool in_suspend = false;
		bool minimized = false;
		bool power_suspended = false;

		// The pointer. `active` is the last WM_ACTIVATEAPP, and `clipping` is
		// whether the clip in force is this window's, so that releasing it
		// never releases another program's.
		bool active = false;
		bool cursor_hidden = false;
		bool cursor_confined = false;
		bool clipping = false;

		unsigned int held_modifiers = 0;
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

		// UTF-16 assembly belongs to this window, between WM_CHAR messages.
		Utf16Input text_input;

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
		cursor_hidden(!options.cursor_visible),
		cursor_confined(options.cursor_confined),
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
		// and nothing said so. AdjustWindowRectExForDpi is the only way to ask
		// what frame this style costs, and it answers zero for the WS_POPUP branch,
		// which is why one call covers both. There is no window to ask for a
		// DPI yet, so this is the system's, and the window is corrected below
		// if the monitor it lands on has another.
		const Vector2I outer = outer_size_for_client(
			options.client_size, style, ex_style, GetDpiForSystem());

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
		// A window created hidden is the exception. Windowed, it gets no such
		// report, because the system sends an overlapped window its first
		// WM_SIZE at its first show; full screen, the report is the requested
		// size, because it is never maximised onto the monitor. Either way the
		// caller's resolution stays the requested size, which is the client
		// it got unless that is below the smallest window the system allows.
		//
		// SW_SHOWDEFAULT is what wWinMain's own show command would have said:
		// it asks for whatever the process that started this one put in its
		// STARTUPINFO, which is where that argument comes from.
		//
		// Before it, a per-monitor-v2 window created on a monitor whose DPI
		// is not the system's has its frame measured again, at its own DPI,
		// so the size report the show fires is already the size asked for.
		if (frame_dpi(this->handle) != GetDpiForSystem())
		{
			const Vector2I corrected = this->outer_size(
				options.client_size);
			SetWindowPos(this->handle, nullptr, 0, 0, corrected.x, corrected.y,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		}

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

		// A clip outlives the window that made it: it is the system's, and
		// left in place it would hold the pointer in a rectangle that is no
		// longer anything.
		if (this->clipping)
		{
			ClipCursor(nullptr);
			this->clipping = false;
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

	void Window::resize_client(const Vector2I& client_size) const
	{
		// Adjusted for whatever frame the window is currently wearing, so the
		// caller gets the client area it asked for rather than that minus a
		// caption. The WM_SIZE this produces reports what the window actually
		// became, which is not always what was asked for - a size past the
		// monitor's comes back clamped.
		const Vector2I outer = this->impl_->outer_size(client_size);
		SetWindowPos(this->impl_->handle, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	// A HIDDEN WINDOW STAYS HIDDEN, here and in leave_fullscreen. ShowWindow
	// is what sizes a shown window, and it shows and activates as it does, so
	// a hidden one is sized with SetWindowPos instead and nothing makes it
	// visible. Whether the window is shown is read from the window at call
	// time rather than kept: a window shown by other means then goes full
	// screen as a shown one does, and no flag can disagree with the screen.
	//
	// WS_VISIBLE IS PART OF THE STYLE, so each replacement below carries it
	// over. Replacing the style without it leaves a window on screen that
	// Windows counts as hidden: nothing it presents is shown, and the next
	// visibility read is wrong.
	void Window::enter_fullscreen() const
	{
		const HWND handle = this->impl_->handle;
		const bool shown = IsWindowVisible(handle) != FALSE;
		SetWindowLongPtr(handle, GWL_STYLE, WS_POPUP | (shown ? WS_VISIBLE : 0));
		SetWindowLongPtr(handle, GWL_EXSTYLE, WS_EX_TOPMOST);
		if (shown)
		{
			SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			ShowWindow(handle, SW_SHOWMAXIMIZED);
			return;
		}

		// Sized as a shown window would be, and not shown. The window state
		// differs - this one is restored rather than maximised - and WM_SIZE
		// reports the two alike, so the size is the whole of what is promised.
		const RECT monitor = monitor_rect(handle);
		SetWindowPos(handle, HWND_TOP, monitor.left, monitor.top,
			monitor.right - monitor.left, monitor.bottom - monitor.top,
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	void Window::leave_fullscreen(const Vector2I& client_size) const
	{
		const HWND handle = this->impl_->handle;
		const bool shown = IsWindowVisible(handle) != FALSE;

		// The style is restored BEFORE the frame arithmetic below, so that
		// arithmetic reads the ordinary window's frame and not WS_POPUP's
		// nothing.
		SetWindowLongPtr(handle, GWL_STYLE,
			WS_OVERLAPPEDWINDOW | (shown ? WS_VISIBLE : 0));
		SetWindowLongPtr(handle, GWL_EXSTYLE, 0);

		const Vector2I outer = this->impl_->outer_size(client_size);

		if (shown)
		{
			ShowWindow(handle, SW_SHOWNORMAL);
			SetWindowPos(handle, HWND_TOP, 0, 0, outer.x, outer.y,
				SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
			return;
		}

		SetWindowPos(handle, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	void Window::set_cursor_visible(bool visible) const
	{
		this->impl_->cursor_hidden = !visible;
		if (this->impl_->handle != nullptr)
		{
			this->impl_->apply_cursor_shape(this->impl_->handle);
		}
	}

	void Window::set_cursor_confined(bool confined) const
	{
		this->impl_->cursor_confined = confined;
		if (this->impl_->handle != nullptr)
		{
			this->impl_->update_cursor_clip(this->impl_->handle);
		}
	}

	void Window::Impl::update_cursor_clip(HWND window)
	{
		const bool wanted = this->cursor_confined && this->active &&
			!this->minimized && !this->in_sizemove &&
			IsWindowVisible(window) != FALSE;

		if (wanted)
		{
			// The client rect in screen pixels. ClipCursor takes the right and
			// bottom edges as exclusive, as GetClientRect gives them.
			RECT client;
			GetClientRect(window, &client);
			MapWindowPoints(window, nullptr,
				reinterpret_cast<POINT*>(&client), 2);
			if (ClipCursor(&client) != FALSE)
			{
				this->clipping = true;
			}
		}
		else if (this->clipping)
		{
			ClipCursor(nullptr);
			this->clipping = false;
		}
	}

	void Window::Impl::apply_cursor_shape(HWND window) const
	{
		POINT point;
		if (GetCursorPos(&point) == FALSE || WindowFromPoint(point) != window)
		{
			return;
		}
		if (SendMessageW(window, WM_NCHITTEST, 0,
			MAKELPARAM(point.x, point.y)) != HTCLIENT)
		{
			return;
		}
		SetCursor(this->cursor_hidden ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
	}

	Vector2I Window::Impl::outer_size(
		const Vector2I& client_size) const
	{
		return outer_size_for_client(client_size,
			static_cast<DWORD>(GetWindowLongPtrW(this->handle, GWL_STYLE)),
			static_cast<DWORD>(GetWindowLongPtrW(this->handle, GWL_EXSTYLE)),
			frame_dpi(this->handle));
	}

	void Window::Impl::update_suspension()
	{
		const bool suspended = this->minimized || this->power_suspended;
		if (suspended == this->in_suspend) { return; }
		this->in_suspend = suspended;
		if (suspended)
		{
			this->held_modifiers = 0;
			this->notify->on_suspending();
		}
		else { this->notify->on_resuming(); }
	}

	bool Window::Impl::update_modifier(Key key, LPARAM l_param, bool down)
	{
		unsigned int first = 0;
		bool right = (l_param & (1LL << 24)) != 0;
		switch (key)
		{
		case Key::shift:
			right = ((static_cast<unsigned long long>(l_param) >> 16) & 0xffu) == 0x36u;
			break;
		case Key::control: first = 2; break;
		case Key::alt: first = 4; break;
		default: return true;
		}

		// Repeated downs set one side rather than adding a second hold. The
		// public key goes up only after both physical sides have gone up.
		const unsigned int bit = 1u << (first + (right ? 1u : 0u));
		if (down)
		{
			this->held_modifiers |= bit;
			return true;
		}
		this->held_modifiers &= ~bit;
		return (this->held_modifiers & (3u << first)) == 0;
	}

	// THE OWNED WINDOW'S MESSAGES, GROUPED BY WHAT THEY ARE ABOUT. Each
	// handler either answers its message or hands it on to DefWindowProc, so a
	// message reaches Windows exactly when its handler does not consume it.

	LRESULT Window::Impl::on_size_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		switch (message)
		{
		case WM_PAINT:
			// While the user drags the window Windows owns the loop, so the only
			// way to keep drawing is from inside the paint message.
			if (this->in_sizemove)
			{
				this->notify->tick();
				break;
			}
			return paint_nothing(window, message, w_param, l_param);

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
			if (!this->in_sizemove)
			{
				this->notify->on_window_moved();
			}
			this->update_cursor_clip(window);
			break;

		case WM_SIZE:
			if (w_param == SIZE_MINIMIZED)
			{
				this->minimized = true;
				this->update_suspension();
			}
			else if (this->minimized)
			{
				this->minimized = false;

				// THE SIZE GOES OUT TOO, and before the resume. A window does
				// not always come back the size it left: one restored from the
				// taskbar into the maximised state it held before, or restored
				// after the monitor changed under it, arrives as one WM_SIZE,
				// the first this branch has seen since the minimise. What it
				// does not deliver is never delivered, and the layout and back
				// buffer would stay at the pre-minimise size.
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
				if (!this->in_sizemove)
				{
					this->notify->on_window_size_changed(
						LOWORD(l_param), HIWORD(l_param));
				}
				this->update_suspension();
			}
			else if (!this->in_sizemove)
			{
				this->notify->on_window_size_changed(
					LOWORD(l_param), HIWORD(l_param));
			}
			this->update_cursor_clip(window);
			break;

		// A confined pointer is let go for the drag, or the player could not
		// reach the edge they meant to pull.
		case WM_ENTERSIZEMOVE:
			this->in_sizemove = true;
			this->update_cursor_clip(window);
			break;

		case WM_EXITSIZEMOVE:
		{
			this->in_sizemove = false;
			this->update_cursor_clip(window);

			RECT client;
			GetClientRect(window, &client);
			this->notify->on_window_size_changed(
				client.right - client.left, client.bottom - client.top);
			break;
		}

		case WM_GETMINMAXINFO:
			if (l_param)
			{
				MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(l_param);
				info->ptMinTrackSize.x = this->min_width;
				info->ptMinTrackSize.y = this->min_height;
			}
			break;

		default:
			break;
		}

		return DefWindowProc(window, message, w_param, l_param);
	}

	LRESULT Window::Impl::on_activation_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		switch (message)
		{
		case WM_KILLFOCUS:
			this->held_modifiers = 0;
			break;

		case WM_ACTIVATEAPP:
			this->active = w_param != 0;
			this->update_cursor_clip(window);
			if (w_param)
			{
				this->notify->on_activated();
			}
			else
			{
				this->held_modifiers = 0;
				this->notify->on_deactivated();
			}
			break;

		case WM_POWERBROADCAST:
			switch (w_param)
			{
			case PBT_APMSUSPEND:
				this->power_suspended = true;
				this->update_suspension();
				return TRUE;
			case PBT_APMRESUMEAUTOMATIC:
			case PBT_APMRESUMESUSPEND:
				this->power_suspended = false;
				this->update_suspension();
				return TRUE;
			default:
				break;
			}
			break;

		default:
			break;
		}

		return DefWindowProc(window, message, w_param, l_param);
	}

	// THE SYS VARIANTS ARE HERE TOO, and then fall through to
	// DefWindowProc rather than being swallowed. Alt is a key like any
	// other and a game may bind it, but Alt+F4, Alt+Space and Alt+Tab are
	// the system's and stay the system's - handling the message is not the
	// same as consuming it. F10 and Alt pressed alone are keys and nothing
	// else: what they would otherwise open is the menu bar, this window has
	// none, and the menu loop would only stall the pump. WM_SYSCOMMAND
	// below is where that is decided.
	//
	// Key repeat needs no filtering. Windows resends WM_KEYDOWN while a
	// key is held; Keyboard::on_key_down sets a bit that is already set,
	// and because the edges are derived from two polled frames rather than
	// from these messages, a repeat cannot manufacture a second press.
	LRESULT Window::Impl::on_key_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
		const Key key = key_from_message(w_param, l_param);
		if (this->update_modifier(key, l_param, down))
		{
			if (down)
			{
				this->notify->on_key_down(key);
			}
			else
			{
				this->notify->on_key_up(key);
			}
		}
		return DefWindowProc(window, message, w_param, l_param);
	}

	// TYPED TEXT, which is the one channel polling cannot rebuild - the
	// shift resolution, the key repeat, the dead keys and the IME have all
	// already happened by the time a character arrives here.
	//
	// WM_SYSCHAR is deliberately NOT handled. It is what Alt+F produces,
	// and treating it as text types an "f" into whatever field is open
	// every time a player reaches for a menu.
	LRESULT Window::Impl::on_char_message(HWND window, WPARAM w_param,
		LPARAM l_param)
	{
		const std::optional<char32_t> codepoint =
			this->text_input.append(static_cast<char16_t>(w_param));
		if (codepoint.has_value())
		{
			this->notify->on_text(*codepoint);
		}
		return DefWindowProc(window, WM_CHAR, w_param, l_param);
	}

	// THE POINTER'S SHAPE OVER THE CLIENT AREA, asked for every time it moves.
	// Hidden is no cursor at all; otherwise DefWindowProc answers with the
	// class's arrow, and over the frame with the frame's own cursors.
	LRESULT Window::Impl::on_cursor_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		if (LOWORD(l_param) == HTCLIENT && this->cursor_hidden)
		{
			SetCursor(nullptr);
			return TRUE;
		}
		return DefWindowProc(window, message, w_param, l_param);
	}

	// NO DOUBLE-CLICK MESSAGES ARRIVE, and that is deliberate rather than
	// missing. The window class above is registered without CS_DBLCLKS, so
	// Windows never substitutes WM_LBUTTONDBLCLK for the second press of a
	// pair - every press is an ordinary button-down and none is lost. What
	// counts as a double click is a policy the game owns, and two presses
	// with their timing is what it needs to decide.
	LRESULT Window::Impl::on_mouse_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		switch (message)
		{
		case WM_MOUSEMOVE:
			this->notify->on_mouse_move(low_word_signed(l_param),
				high_word_signed(l_param));
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
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
				if (this->held_buttons == 0)
				{
					SetCapture(window);
				}
				++this->held_buttons;

				this->notify->on_mouse_button_down(button);
			}
			break;
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		{
			const MouseButton button =
				message == WM_LBUTTONUP ? MouseButton::left :
				message == WM_RBUTTONUP ? MouseButton::right :
				message == WM_MBUTTONUP ? MouseButton::middle :
				x_button(w_param);

			if (button != MouseButton::none)
			{
				if (this->held_buttons > 0)
				{
					--this->held_buttons;
					if (this->held_buttons == 0)
					{
						ReleaseCapture();
					}
				}

				this->notify->on_mouse_button_up(button);
			}
			break;
		}

		case WM_CAPTURECHANGED:
			// Normal last-button release has already reduced the count to zero.
			// Transfer/cancellation can happen while the application keeps focus.
			if (this->held_buttons > 0)
			{
				this->held_buttons = 0;
				this->notify->on_mouse_capture_lost();
			}
			break;

		case WM_MOUSEWHEEL:
			this->notify->on_mouse_wheel(
				static_cast<float>(static_cast<short>(HIWORD(w_param))) /
				static_cast<float>(WHEEL_DELTA));
			break;

		case WM_MOUSEHWHEEL:
			this->notify->on_mouse_wheel_horizontal(
				static_cast<float>(static_cast<short>(HIWORD(w_param))) /
				static_cast<float>(WHEEL_DELTA));
			break;

		default:
			break;
		}

		return mouse_default(window, message, w_param, l_param);
	}

	// TOUCH, AND ONLY TOUCH. A pen and the mouse fall through to
	// DefWindowProc, which turns them into the mouse messages above - a pen
	// is the mouse in this engine (pointer_input.h says why). A touch
	// contact is handled here and not passed on, so Windows makes no
	// mouse messages of it, and none of its gestures either: a press and
	// hold is a point held, not a right click.
	//
	// The position is in screen pixels and is turned into client pixels
	// here, signed for the reason low_word_signed gives: a touch that
	// started in the window is captured to it and followed off the edge.
	//
	// WM_POINTERCAPTURECHANGED is answered whatever the pointer is,
	// because by then it may be no kind Windows will still name, and a
	// cancel for a point the touch device does not hold does nothing.
	LRESULT Window::Impl::on_pointer_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		const UINT32 pointer = GET_POINTERID_WPARAM(w_param);
		if (message == WM_POINTERCAPTURECHANGED)
		{
			this->notify->on_touch_cancel(pointer);
			return DefWindowProc(window, message, w_param, l_param);
		}

		POINTER_INPUT_TYPE type = PT_POINTER;
		if (GetPointerType(pointer, &type) == 0 || type != PT_TOUCH)
		{
			return DefWindowProc(window, message, w_param, l_param);
		}

		POINT position = { low_word_signed(l_param),
			high_word_signed(l_param) };
		ScreenToClient(window, &position);

		switch (touch_message(message, static_cast<uint64_t>(w_param)))
		{
		case TouchMessage::down:
			this->notify->on_touch_down(pointer, position.x, position.y);
			break;
		case TouchMessage::move:
			this->notify->on_touch_move(pointer, position.x, position.y);
			break;
		case TouchMessage::up:
			this->notify->on_touch_up(pointer, position.x, position.y);
			break;
		case TouchMessage::cancel:
			this->notify->on_touch_cancel(pointer);
			break;
		case TouchMessage::none:
			break;
		}
		return 0;
	}

	LRESULT Window::Impl::on_destroy_message(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		switch (message)
		{
		// ONLY WHILE OWNED. The user data is zero for a window the destructor
		// is taking down, and that destructor says why the quit must not be
		// posted then. For close() and for the user's own X it is set, and
		// this is the quit the pump is waiting for.
		case WM_DESTROY:
			if (this->clipping)
			{
				ClipCursor(nullptr);
				this->clipping = false;
			}
			PostQuitMessage(0);
			break;

		// The last message a window receives. The handle is invalid the
		// moment this returns, so the object stops naming it here - which is
		// what lets the destructor tell an ordinary exit from an unwind, and
		// makes a second close() a no-op rather than a call on a stale HWND.
		case WM_NCDESTROY:
			this->handle = nullptr;
			SetWindowLongPtr(window, GWLP_USERDATA, 0);
			break;

		default:
			break;
		}

		return DefWindowProc(window, message, w_param, l_param);
	}

	// Every message either forwards through WindowNotify or is Windows
	// housekeeping. There is nothing game-specific here, which is the reason
	// it is in the engine and not copied into every project's main.cpp.
	LRESULT CALLBACK Window::Impl::window_proc(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		Impl* self = reinterpret_cast<Impl*>(
			GetWindowLongPtr(window, GWLP_USERDATA));

		// ONE TOUCH IS ONE INPUT. The touch itself arrives as WM_POINTER
		// below, and is handled there without DefWindowProc, which is what
		// stops Windows making mouse messages of it. This is the second half,
		// for any copy that is made anyway: it would otherwise be a click in
		// the same frame as the tap, and on a widget the two would press it
		// twice.
		if (is_mouse_input(message) &&
			mouse_message_is_from_touch(GetMessageExtraInfo()))
		{
			return DefWindowProc(window, message, w_param, l_param);
		}

		// Messages that need no Impl: they arrive before WM_CREATE has stored
		// it, or after WM_NCDESTROY or the destructor has cleared it, as well
		// as while it is set.
		switch (message)
		{
		case WM_CREATE:
			if (l_param)
			{
				LPCREATESTRUCTW params = reinterpret_cast<LPCREATESTRUCTW>(l_param);
				SetWindowLongPtr(window, GWLP_USERDATA,
					reinterpret_cast<LONG_PTR>(params->lpCreateParams));
			}
			return DefWindowProc(window, message, w_param, l_param);

		// A DPI CHANGE KEEPS THE CLIENT PIXELS. The resolution is the game's,
		// in physical pixels (Application::set_resolution), so a window moved
		// to a monitor of another density, or on one whose scale is changed,
		// keeps its client size and only its frame changes. Under per-monitor
		// awareness v2 the system asks first, here, for the outer size at the
		// new DPI; answering the size that keeps the client makes the rect
		// WM_DPICHANGED then suggests agree, which is also what keeps a drag
		// across a monitor edge from oscillating. A full-screen WS_POPUP has
		// no frame, so its answer is its current size and it stays as it is.
		// A minimised window has no client to keep, and is left to the system.
		// Neither message is sent to a window that is not per-monitor aware.
		case WM_GETDPISCALEDSIZE:
			return self ? dpi_scaled_size(window, w_param, l_param) : FALSE;

		// Under v2 the rect is applied as the system suggests it. The WM_SIZE
		// that produces reports the client as every other resize does; during
		// a drag in_sizemove holds it until WM_EXITSIZEMOVE. Under per-monitor
		// v1 the system neither asks first nor scales the frame, so its
		// suggested rect keeps the window's logical size and would rescale
		// the client; the message is left to DefWindowProc, which does not
		// resize a top-level window, and the client keeps its pixels.
		case WM_DPICHANGED:
			return dpi_changed(window, message, w_param, l_param);

		// F10, or Alt pressed and released alone. Passed on, DefWindowProc
		// enters a modal loop on the system menu: PeekMessage stops returning
		// to pump_until_quit, tick() stops, the next keys go to a menu nobody
		// can see, and nothing tells the game. Swallowed, the keys have already
		// reached the keyboard as keys. Alt+Space names its key and still
		// opens the system menu, which is how a keyboard reaches Move and Size.
		case WM_SYSCOMMAND:
			if (is_bare_menu_activation(static_cast<uint64_t>(w_param),
				static_cast<intptr_t>(l_param)))
			{
				return 0;
			}
			return DefWindowProc(window, message, w_param, l_param);

		case WM_MENUCHAR:
			// A menu is active and the key pressed matches no mnemonic. Swallow it
			// so Windows does not beep.
			return MAKELRESULT(0, MNC_CLOSE);

		default:
			break;
		}

		if (self == nullptr)
		{
			switch (message)
			{
			case WM_PAINT:
				return paint_nothing(window, message, w_param, l_param);
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
				return mouse_default(window, message, w_param, l_param);
			default:
				return DefWindowProc(window, message, w_param, l_param);
			}
		}

		switch (message)
		{
		case WM_PAINT:
		case WM_MOVE:
		case WM_SIZE:
		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
		case WM_GETMINMAXINFO:
			return self->on_size_message(window, message, w_param, l_param);

		case WM_KILLFOCUS:
		case WM_ACTIVATEAPP:
		case WM_POWERBROADCAST:
			return self->on_activation_message(window, message, w_param,
				l_param);

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			return self->on_key_message(window, message, w_param, l_param);

		case WM_CHAR:
			return self->on_char_message(window, w_param, l_param);

		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		case WM_CAPTURECHANGED:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			return self->on_mouse_message(window, message, w_param, l_param);

		case WM_SETCURSOR:
			return self->on_cursor_message(window, message, w_param, l_param);

		case WM_POINTERDOWN:
		case WM_POINTERUPDATE:
		case WM_POINTERUP:
		case WM_POINTERCAPTURECHANGED:
			return self->on_pointer_message(window, message, w_param, l_param);

		case WM_DESTROY:
		case WM_NCDESTROY:
			return self->on_destroy_message(window, message, w_param, l_param);

		default:
			return DefWindowProc(window, message, w_param, l_param);
		}
	}
}

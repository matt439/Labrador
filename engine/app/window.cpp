#include "engine/app/window.h"

#include <stdexcept>
#include <tuple>

namespace labrador
{
	namespace
	{
		// THE TWO VOCABULARIES MEET HERE AND NOWHERE ELSE. Above this file a
		// key is a Key, which is an engine name for a position on a keyboard;
		// below it a key is a VK_ constant, which is Microsoft's. Keeping the
		// translation in one function is what lets engine/input/keyboard.h be
		// a file with no platform in it at all.
		//
		// Unknown codes answer Key::none, which the device drops. A keyboard
		// with a key this engine has never heard of is not an error - it is a
		// keyboard (T6: loud is for broken contracts, not for the world being
		// the world).
		Key key_from_virtual_key(WPARAM w_param)
		{
			const int code = static_cast<int>(w_param);

			const auto offset = [](Key first, int distance)
			{
				return static_cast<Key>(
					static_cast<unsigned int>(first) +
					static_cast<unsigned int>(distance));
			};

			// FOUR RUNS THAT ARE CONTIGUOUS IN BOTH VOCABULARIES, which is
			// what keeps this function from being a hundred cases long. The
			// enumerators were declared in these orders on purpose; a Key
			// added into the middle of one of them breaks this arithmetic
			// silently, which is what the tests beside this file check.
			if (code >= 'A' && code <= 'Z')
			{
				return offset(Key::a, code - 'A');
			}
			if (code >= '0' && code <= '9')
			{
				return offset(Key::digit_0, code - '0');
			}
			if (code >= VK_F1 && code <= VK_F12)
			{
				return offset(Key::f1, code - VK_F1);
			}
			if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9)
			{
				return offset(Key::numpad_0, code - VK_NUMPAD0);
			}

			switch (code)
			{
			case VK_ESCAPE:		return Key::escape;
			case VK_TAB:		return Key::tab;
			case VK_CAPITAL:	return Key::caps_lock;
			case VK_SPACE:		return Key::space;
			case VK_RETURN:		return Key::enter;
			case VK_BACK:		return Key::backspace;

			// The sided codes fold onto the generic ones, which is the limit
			// keyboard.h documents rather than an omission here. A message
			// pump sees VK_SHIFT for both, and the side is in the scan code -
			// a second lookup no client has asked for (T1).
			case VK_SHIFT:
			case VK_LSHIFT:
			case VK_RSHIFT:		return Key::shift;
			case VK_CONTROL:
			case VK_LCONTROL:
			case VK_RCONTROL:	return Key::control;
			case VK_MENU:
			case VK_LMENU:
			case VK_RMENU:		return Key::alt;

			case VK_INSERT:		return Key::insert;
			case VK_DELETE:		return Key::del;
			case VK_HOME:		return Key::home;
			case VK_END:		return Key::end;
			case VK_PRIOR:		return Key::page_up;
			case VK_NEXT:		return Key::page_down;

			case VK_LEFT:		return Key::left;
			case VK_RIGHT:		return Key::right;
			case VK_UP:			return Key::up;
			case VK_DOWN:		return Key::down;

			case VK_SNAPSHOT:	return Key::print_screen;
			case VK_SCROLL:		return Key::scroll_lock;
			case VK_PAUSE:		return Key::pause;
			case VK_NUMLOCK:	return Key::num_lock;

			// The OEM codes are positions on a US layout and are documented as
			// such. A different layout puts a different character there, which
			// is exactly why Key is a position and typed() is the other
			// question.
			case VK_OEM_MINUS:	return Key::minus;
			case VK_OEM_PLUS:	return Key::equals;
			case VK_OEM_4:		return Key::left_bracket;
			case VK_OEM_6:		return Key::right_bracket;
			case VK_OEM_5:		return Key::backslash;
			case VK_OEM_1:		return Key::semicolon;
			case VK_OEM_7:		return Key::apostrophe;
			case VK_OEM_3:		return Key::grave;
			case VK_OEM_COMMA:	return Key::comma;
			case VK_OEM_PERIOD:	return Key::period;
			case VK_OEM_2:		return Key::slash;

			case VK_ADD:		return Key::numpad_add;
			case VK_SUBTRACT:	return Key::numpad_subtract;
			case VK_MULTIPLY:	return Key::numpad_multiply;
			case VK_DIVIDE:		return Key::numpad_divide;
			case VK_DECIMAL:	return Key::numpad_decimal;

			default:			return Key::none;
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
	}

	Window::Window(HINSTANCE instance, int show_command,
		const WindowOptions& options, WindowNotify* notify) :
		notify_(notify),
		min_width_(options.min_window_width),
		min_height_(options.min_window_height)
	{
		WNDCLASSEXW window_class = {};
		window_class.cbSize = sizeof(WNDCLASSEXW);
		window_class.style = CS_HREDRAW | CS_VREDRAW;
		window_class.lpfnWndProc = window_proc;
		window_class.hInstance = instance;
		window_class.hIcon = LoadIconW(instance, L"IDI_ICON");
		window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		window_class.lpszClassName = options.window_class_name.c_str();
		window_class.hIconSm = LoadIconW(instance, L"IDI_ICON");

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

		// This Window rides in as the create parameter and is stashed in the
		// window's user data by WM_CREATE, so window_proc can find it without
		// a global. Every member window_proc reads is already initialised -
		// see the declaration order in the header.
		this->handle_ = CreateWindowExW(ex_style,
			options.window_class_name.c_str(),
			options.window_title.c_str(), style,
			CW_USEDEFAULT, CW_USEDEFAULT, outer.x, outer.y,
			nullptr, nullptr, instance, this);

		if (this->handle_ == nullptr)
		{
			throw std::runtime_error("Could not create the window.");
		}

		// AND THE WM_SIZE THIS PRODUCES IS WORTH SOMETHING. It arrives before
		// the device exists, so the renderer half of on_window_size_changed
		// still does nothing - DeviceResources::WindowSizeChanged returns early
		// with no window set. The resolution-manager half does not, so by the
		// time the caller reads its resolution back for create_device, that is
		// the client size the window really got. It matters most for the
		// full-screen branch, where SW_SHOWMAXIMIZED decides the size and
		// nothing here knows it: the swap chain used to be created at the saved
		// preset and then stretched non-uniformly to the monitor
		// (DXGI_SCALING_STRETCH), which is the one form of this bug the shipped
		// sample hits.
		ShowWindow(this->handle_,
			options.fullscreen ? SW_SHOWMAXIMIZED : show_command);
	}

	// No DestroyWindow here, deliberately. By the time this runs the pump has
	// returned, and it only returns on WM_QUIT, which came from
	// PostQuitMessage inside WM_DESTROY - so the handle is already gone and
	// destroying it again would be an error on a stale HWND.
	Window::~Window() = default;

	HWND Window::handle() const
	{
		return this->handle_;
	}

	int Window::exit_code() const
	{
		return this->exit_code_;
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
				this->notify_->tick();
			}
		}
		this->exit_code_ = static_cast<int>(message.wParam);
	}

	void Window::close() const
	{
		if (this->handle_ != nullptr)
		{
			DestroyWindow(this->handle_);
		}
	}

	void Window::resize_client(const mattmath::Vector2I& client_size) const
	{
		// Adjusted for whatever frame the window is currently wearing, so the
		// caller gets the client area it asked for rather than that minus a
		// caption. The WM_SIZE this produces reports what the window actually
		// became, which is not always what was asked for - a size past the
		// monitor's comes back clamped.
		const mattmath::Vector2I outer = this->outer_size_for_client(client_size);
		SetWindowPos(this->handle_, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER);
	}

	void Window::enter_fullscreen() const
	{
		SetWindowLongPtr(this->handle_, GWL_STYLE, WS_POPUP);
		SetWindowLongPtr(this->handle_, GWL_EXSTYLE, WS_EX_TOPMOST);
		SetWindowPos(this->handle_, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowWindow(this->handle_, SW_SHOWMAXIMIZED);
	}

	void Window::leave_fullscreen(const mattmath::Vector2I& client_size) const
	{
		// The style is restored BEFORE the frame arithmetic below, so that
		// arithmetic reads the ordinary window's frame and not WS_POPUP's
		// nothing.
		SetWindowLongPtr(this->handle_, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowLongPtr(this->handle_, GWL_EXSTYLE, 0);

		const mattmath::Vector2I outer = this->outer_size_for_client(client_size);

		ShowWindow(this->handle_, SW_SHOWNORMAL);
		SetWindowPos(this->handle_, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	mattmath::Vector2I Window::outer_size_for_client(
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

	mattmath::Vector2I Window::outer_size_for_client(
		const mattmath::Vector2I& client_size) const
	{
		return outer_size_for_client(client_size,
			static_cast<DWORD>(GetWindowLongPtrW(this->handle_, GWL_STYLE)),
			static_cast<DWORD>(GetWindowLongPtrW(this->handle_, GWL_EXSTYLE)));
	}

	// Every message either forwards through WindowNotify or is Windows
	// housekeeping. There is nothing game-specific here, which is the reason
	// it is in the engine and not copied into every project's main.cpp.
	LRESULT CALLBACK Window::window_proc(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		auto* self = reinterpret_cast<Window*>(
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
			if (self && self->in_sizemove_)
			{
				self->notify_->tick();
			}
			else
			{
				PAINTSTRUCT paint;
				std::ignore = BeginPaint(window, &paint);
				EndPaint(window, &paint);
			}
			break;

		case WM_MOVE:
			// GATED ON !in_sizemove_, THE SAME WAY WM_SIZE BELOW IS, and the
			// two have to be gated together. Dragging the LEFT or TOP edge
			// moves the origin as well as the size, so Windows sends WM_MOVE
			// per step of the drag - and this handler asks the renderer for a
			// size, which is exactly the term the drag is allowed to be out of
			// date about. Two of the backends answer it from a swap chain and
			// so answer the size they were last told, making the call a
			// self-comparison that changes nothing; the GL backend answers it
			// from the window (renderer.h, back_buffer_size), so every step
			// used to hand it the already-current client rect. That cost a
			// viewport, a clear and a per-view reset per mouse-move step, and
			// then made gl answer "nothing changed" to the WM_EXITSIZEMOVE
			// below - the one message that ends a resize, which the other
			// backends answer true to.
			if (self && !self->in_sizemove_)
			{
				self->notify_->on_window_moved();
			}
			break;

		case WM_SIZE:
			// in_suspend_ and minimized_ are the two flags here that call no
			// Win32 at all. They live on the window anyway, because between
			// them they collapse two independent Windows suspend sources - a
			// minimise and a power-suspend broadcast - into the single
			// on_suspending/on_resuming pair the owner sees. That collapsing
			// is message translation, which is this file's whole job.
			if (self && w_param == SIZE_MINIMIZED)
			{
				if (!self->minimized_)
				{
					self->minimized_ = true;
					if (!self->in_suspend_)
					{
						self->notify_->on_suspending();
					}
					self->in_suspend_ = true;
				}
			}
			else if (self && self->minimized_)
			{
				self->minimized_ = false;
				if (self->in_suspend_)
				{
					self->notify_->on_resuming();
				}
				self->in_suspend_ = false;
			}
			else if (self && !self->in_sizemove_)
			{
				self->notify_->on_window_size_changed(
					LOWORD(l_param), HIWORD(l_param));
			}
			break;

		case WM_ENTERSIZEMOVE:
			if (self)
			{
				self->in_sizemove_ = true;
			}
			break;

		case WM_EXITSIZEMOVE:
			if (self)
			{
				self->in_sizemove_ = false;

				RECT client;
				GetClientRect(window, &client);
				self->notify_->on_window_size_changed(
					client.right - client.left, client.bottom - client.top);
			}
			break;

		case WM_GETMINMAXINFO:
			if (l_param && self)
			{
				auto info = reinterpret_cast<MINMAXINFO*>(l_param);
				info->ptMinTrackSize.x = self->min_width_;
				info->ptMinTrackSize.y = self->min_height_;
			}
			break;

		case WM_ACTIVATEAPP:
			if (self)
			{
				if (w_param)
				{
					self->notify_->on_activated();
				}
				else
				{
					self->notify_->on_deactivated();
				}
			}
			break;

		case WM_POWERBROADCAST:
			switch (w_param)
			{
			case PBT_APMQUERYSUSPEND:
				if (self)
				{
					if (!self->in_suspend_)
					{
						self->notify_->on_suspending();
					}
					self->in_suspend_ = true;
				}
				return TRUE;

			case PBT_APMRESUMESUSPEND:
				if (self && !self->minimized_)
				{
					if (self->in_suspend_)
					{
						self->notify_->on_resuming();
					}
					self->in_suspend_ = false;
				}
				return TRUE;
			default:
				break;
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
				self->notify_->on_key_down(key_from_virtual_key(w_param));
			}
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (self)
			{
				self->notify_->on_key_up(key_from_virtual_key(w_param));
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
					self->pending_high_surrogate_ = unit;
					break;
				}

				char32_t codepoint = static_cast<char32_t>(unit);

				if (unit >= 0xDC00 && unit <= 0xDFFF &&
					self->pending_high_surrogate_ != 0)
				{
					codepoint = 0x10000u +
						((static_cast<char32_t>(
							self->pending_high_surrogate_) - 0xD800u) << 10) +
						(static_cast<char32_t>(unit) - 0xDC00u);
				}

				// Cleared whatever happened, including the case where a low
				// half never came: a stale high surrogate joined to the next
				// unrelated character would corrupt a good one as well as the
				// lost one. An unpaired half passed on as-is becomes U+FFFD
				// inside Keyboard::on_text, which is where that rule lives.
				self->pending_high_surrogate_ = 0;

				self->notify_->on_text(codepoint);
			}
			break;

		case WM_MOUSEMOVE:
			if (self)
			{
				self->notify_->on_mouse_move(low_word_signed(l_param),
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
					// Capture on the way out of zero. See held_buttons_ in the
					// header for why this counts rather than flags.
					if (self->held_buttons_ == 0)
					{
						SetCapture(window);
					}
					++self->held_buttons_;

					self->notify_->on_mouse_button_down(button);
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
					if (self->held_buttons_ > 0)
					{
						--self->held_buttons_;
						if (self->held_buttons_ == 0)
						{
							ReleaseCapture();
						}
					}

					self->notify_->on_mouse_button_up(button);
				}
			}
			if (message == WM_XBUTTONUP)
			{
				return TRUE;
			}
			break;

		case WM_CAPTURECHANGED:
			// Something took the capture away - a system drag, an Alt-Tab, a
			// modal the driver put up. The count has to go with it or the next
			// SetCapture never happens, because the count would never return
			// to zero to trigger one. The buttons themselves are cleared by
			// Mouse::set_focused when the deactivation arrives.
			if (self)
			{
				self->held_buttons_ = 0;
			}
			break;

		case WM_MOUSEWHEEL:
			if (self)
			{
				self->notify_->on_mouse_wheel(
					static_cast<float>(
						static_cast<short>(HIWORD(w_param))) /
					static_cast<float>(WHEEL_DELTA));
			}
			break;

		case WM_MOUSEHWHEEL:
			if (self)
			{
				self->notify_->on_mouse_wheel_horizontal(
					static_cast<float>(
						static_cast<short>(HIWORD(w_param))) /
					static_cast<float>(WHEEL_DELTA));
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
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

#include "engine/app/window.h"

#include <stdexcept>
#include <tuple>

namespace artattack
{
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

		case WM_DISPLAYCHANGE:
			if (self)
			{
				self->notify_->on_display_change();
			}
			break;

		case WM_MOVE:
			if (self)
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

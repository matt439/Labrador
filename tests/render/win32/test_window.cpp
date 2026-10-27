#include "tests/render/test_window.h"

#include <Windows.h>

#include <stdexcept>

namespace render_tests
{
	// A popup window, registered once per process on first use and never
	// shown: there is no ShowWindow call anywhere in the tests, so a run puts
	// nothing on screen and steals no focus. WS_POPUP has no frame, so the
	// window's size is its client area's, which is the size create_device is
	// told and the size a resize asks for.
	struct TestWindow::Impl
	{
		HWND window = nullptr;
	};

	namespace
	{
		const wchar_t* const class_name = L"LabradorRenderTestWindow";

		bool register_class()
		{
			static const ATOM atom = []() -> ATOM
				{
					WNDCLASSEXW window_class = {};
					window_class.cbSize = sizeof(WNDCLASSEXW);
					window_class.lpfnWndProc = DefWindowProcW;
					window_class.hInstance = GetModuleHandleW(nullptr);
					window_class.lpszClassName = class_name;
					return RegisterClassExW(&window_class);
				}();
			return atom != 0;
		}
	}

	TestWindow::TestWindow(int width, int height)
		: impl_(std::make_unique<Impl>())
	{
		if (!register_class())
		{
			throw std::runtime_error(
				"The render tests' window class would not register.");
		}

		this->impl_->window = CreateWindowExW(0, class_name, L"render tests",
			WS_POPUP, 0, 0, width, height, nullptr, nullptr,
			GetModuleHandleW(nullptr), nullptr);
		if (this->impl_->window == nullptr)
		{
			throw std::runtime_error(
				"The render tests' hidden window would not open.");
		}
	}

	TestWindow::~TestWindow()
	{
		DestroyWindow(this->impl_->window);
	}

	void* TestWindow::handle() const
	{
		return this->impl_->window;
	}

	bool TestWindow::resize(int width, int height)
	{
		return SetWindowPos(this->impl_->window, nullptr, 0, 0, width, height,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
	}
}

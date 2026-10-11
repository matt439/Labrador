#pragma once

#include <memory>

namespace render_tests
{
	// Something create_device can draw into that nobody sees: a Win32 window
	// that is never shown, or a canvas on the test page. The two files that
	// define it are tests/render/win32/test_window.cpp and
	// tests/render/web/test_window.cpp, and tests/render/CMakeLists.txt
	// compiles exactly one, which is the rule the engine's own platform folders
	// live under and the reason this is a source list rather than an #ifdef.
	//
	// The only thing either one promises is what the seam asks of a window: a
	// handle create_device takes as a void*, whose drawable area is the size
	// asked for, and a resize that changes that area and tells the renderer
	// nothing (Harness::resize_window says why that is the case worth having).
	class TestWindow
	{
	public:
		// Throws std::runtime_error if the platform will not make one.
		TestWindow(int width, int height);
		~TestWindow();

		TestWindow(const TestWindow&) = delete;
		TestWindow& operator=(const TestWindow&) = delete;

		// An HWND, or the canvas's html5 target - whatever create_device is
		// handed on this platform.
		void* handle() const;

		// Whether the drawable area is now width by height.
		bool resize(int width, int height);

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}

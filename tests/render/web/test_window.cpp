#include "tests/render/test_window.h"

#include <emscripten/em_js.h>
#include <emscripten/html5.h>

#include <stdexcept>
#include <string>

// A canvas of its own for every window, appended to the test page and removed
// when the window goes.
//
// ONE CANVAS PER WINDOW, NOT ONE FOR THE RUN, because a canvas has exactly one
// WebGL context for its whole life: getContext answers the same object every
// time it is asked, so a second device made on the same canvas would inherit
// whatever GL state the first one left - its blend, its bindings, its objects -
// where every desktop case starts from a context nobody has touched. A fresh
// element is what a fresh HWND is on Windows.
//
// The drawing buffer is the canvas's width and height attributes, which is
// what create_device is told and what a resize changes. Its CSS box is never
// read: nothing on this page is measured, because no web Window exists here to
// measure it (engine/app/web/window.cpp is the thing that does in a game).

EM_JS(int, labrador_test_canvas_create, (int width, int height), {
	var next = (Module["labradorTestCanvases"] || 0) + 1;
	Module["labradorTestCanvases"] = next;
	var canvas = document.createElement("canvas");
	canvas.id = "labrador-test-canvas-" + next;
	canvas.width = width;
	canvas.height = height;
	document.body.appendChild(canvas);
	return next;
});

// AND THE CONTEXT IS GIVEN BACK BEFORE THE CANVAS GOES. Destroying an
// Emscripten context only forgets its handle; the browser's context lives
// until it is collected, and a browser keeps sixteen or so at once and evicts
// the oldest past that, with a warning per eviction. A run makes one per case,
// so without this a passing run warns about contexts nothing is using once
// for every case past the sixteenth. WEBGL_lose_context is the one call that releases one now, and
// getContext hands back the canvas's existing context rather than a new one.
EM_JS(void, labrador_test_canvas_remove, (int number), {
	var canvas = document.getElementById("labrador-test-canvas-" + number);
	if (!canvas) {
		return;
	}
	var context = canvas.getContext("webgl2");
	var release = context ? context.getExtension("WEBGL_lose_context") : null;
	if (release) {
		release.loseContext();
	}
	canvas.remove();
});

namespace render_tests
{
	struct TestWindow::Impl
	{
		int number = 0;
		// The html5 target, a CSS selector naming the canvas by id. Held here
		// because create_device is handed a pointer into it.
		std::string target;
	};

	TestWindow::TestWindow(int width, int height)
		: impl_(std::make_unique<Impl>())
	{
		this->impl_->number = labrador_test_canvas_create(width, height);
		if (this->impl_->number <= 0)
		{
			throw std::runtime_error(
				"The render tests' canvas would not attach to the page.");
		}
		this->impl_->target =
			"#labrador-test-canvas-" + std::to_string(this->impl_->number);
	}

	TestWindow::~TestWindow()
	{
		labrador_test_canvas_remove(this->impl_->number);
	}

	void* TestWindow::handle() const
	{
		return const_cast<char*>(this->impl_->target.c_str());
	}

	bool TestWindow::resize(int width, int height)
	{
		// The drawing buffer follows at once, as a popup's client area does on
		// Windows, and the renderer reads it from the context rather than from
		// anything it was told (gl/web_context.cpp, drawable_size).
		return emscripten_set_canvas_element_size(this->impl_->target.c_str(),
			width, height) == EMSCRIPTEN_RESULT_SUCCESS;
	}
}

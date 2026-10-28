#include "engine/app/window.h"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

// THE WEB WINDOW, DRIVEN BY A BROWSER. The page half of WebWindowTests: it
// takes the page's canvas with a real labrador::Window and writes down, in
// order, every call that window makes into its WindowNotify. It asserts
// nothing. A runner dispatches real input at the page: in Chromium,
// tests/render/web/run_page.mjs over the DevTools protocol, and in Firefox,
// tests/render/web/run_firefox.mjs over WebDriver BiDi. window_input.mjs
// beside this file says what each input must have produced - so the input and
// what it means are read side by side, in one file, rather than split between
// a script that presses keys and a module that guesses when they arrived.
//
// It is the browser's counterpart to window_tests.cpp, which drives a hidden
// Win32 window through its message procedure. AppTests under Node reaches the
// DOM translation (dom_events.h) but not the window's state - focus, capture,
// visibility, the size it measures - because Node has no DOM to dispatch an
// event from.
//
// THE LOG IS Module.labradorWindowLog, one string per call, and
// window_input.mjs reads and empties it. Two entries are not calls:
//
// - "tick" is written once for a run of frames rather than once per frame,
//   so the log stays readable and a frame still shows where it fell.
// - "tick-unreported WxH" is a frame that ran with a drawing buffer the owner
//   was never told about. The window measures the canvas before every tick
//   and must report a change before the tick that would draw into it, so this
//   line in a log is the failure of that rule however the page was resized.

EM_JS(void, web_window_test_record, (const char* entry), {
	(Module.labradorWindowLog = Module.labradorWindowLog || []).push(
		UTF8ToString(entry));
});

EM_JS(void, web_window_test_ready, (), {
	Module.labradorWindowReady = true;
});

namespace
{
	const char* button_name(labrador::MouseButton button)
	{
		switch (button)
		{
		case labrador::MouseButton::left: return "left";
		case labrador::MouseButton::right: return "right";
		case labrador::MouseButton::middle: return "middle";
		default: return "other";
		}
	}

	void record(const std::string& entry)
	{
		web_window_test_record(entry.c_str());
	}

	std::string number(double value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%g", value);
		return text;
	}

	class Recorder final : public labrador::WindowNotify
	{
	public:
		void tick() override
		{
			int width = 0;
			int height = 0;
			emscripten_get_canvas_element_size("!canvas", &width, &height);
			if (width != this->width_ || height != this->height_)
			{
				record("tick-unreported " + std::to_string(width) + "x" +
					std::to_string(height));
			}
			else if (!this->ticked_)
			{
				record("tick");
			}
			this->ticked_ = true;
		}

		void on_activated() override { this->note("activated"); }
		void on_deactivated() override { this->note("deactivated"); }
		void on_suspending() override { this->note("suspending"); }
		void on_resuming() override { this->note("resuming"); }
		void on_keyboard_gained() override { this->note("keyboard_gained"); }
		void on_keyboard_lost() override { this->note("keyboard_lost"); }

		void on_fullscreen_changed(bool fullscreen) override
		{
			this->note(fullscreen ? "fullscreen 1" : "fullscreen 0");
		}

		void on_window_moved() const override { this->note("moved"); }

		void on_window_size_changed(int width, int height) override
		{
			this->width_ = width;
			this->height_ = height;
			this->note("size " + std::to_string(width) + " " +
				std::to_string(height));
		}

		void on_key_down(labrador::Key key) const override
		{
			this->note("key_down " + std::string(labrador::key_name(key)));
		}

		void on_key_up(labrador::Key key) const override
		{
			this->note("key_up " + std::string(labrador::key_name(key)));
		}

		void on_text(char32_t codepoint) const override
		{
			this->note("text " +
				std::to_string(static_cast<uint32_t>(codepoint)));
		}

		void on_mouse_move(int x, int y) const override
		{
			this->note("mouse_move " + std::to_string(x) + " " +
				std::to_string(y));
		}

		void on_mouse_button_down(labrador::MouseButton button) const override
		{
			this->note(std::string("button_down ") + button_name(button));
		}

		void on_mouse_button_up(labrador::MouseButton button) const override
		{
			this->note(std::string("button_up ") + button_name(button));
		}

		void on_mouse_capture_lost() const override
		{
			this->note("capture_lost");
		}

		void on_mouse_wheel(float notches) const override
		{
			this->note("wheel " + number(notches));
		}

		void on_mouse_wheel_horizontal(float notches) const override
		{
			this->note("hwheel " + number(notches));
		}

		void on_touch_down(uint32_t point, int x, int y) const override
		{
			this->note("touch_down " + std::to_string(point) + " " +
				std::to_string(x) + " " + std::to_string(y));
		}

		void on_touch_move(uint32_t point, int x, int y) const override
		{
			this->note("touch_move " + std::to_string(point) + " " +
				std::to_string(x) + " " + std::to_string(y));
		}

		void on_touch_up(uint32_t point, int x, int y) const override
		{
			this->note("touch_up " + std::to_string(point) + " " +
				std::to_string(x) + " " + std::to_string(y));
		}

		void on_touch_cancel(uint32_t point) const override
		{
			this->note("touch_cancel " + std::to_string(point));
		}

	private:
		// Every call but tick ends a run of frames, so the next frame is
		// written down again.
		void note(const std::string& entry) const
		{
			record(entry);
			this->ticked_ = false;
		}

		int width_ = 0;
		int height_ = 0;
		mutable bool ticked_ = false;
	};

	// Static, because in a browser pump_until_quit unwinds main's stack
	// before the first frame (window.h), and both are called from frames.
	Recorder recorder;
	std::unique_ptr<labrador::Window> window;
}

// Closes the window from the page, as a game's quit would, so that the input
// script can show that nothing reaches the owner afterwards.
extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_close()
{
	if (window)
	{
		window->close();
	}
}

int main()
{
	try
	{
		labrador::WindowOptions options;
		// Smaller than the box the page gives the canvas, so the first report
		// is visibly the measured box rather than the size asked for.
		options.client_size = labrador::Vector2I(320, 200);
		window = std::make_unique<labrador::Window>(options, &recorder);
	}
	catch (const std::exception& error)
	{
		// An abort rather than a return, which settles the page's
		// labradorExit at once: the runtime outlives main here, so a
		// returned code would reach nobody.
		std::fprintf(stderr, "WebWindowTests: %s\n", error.what());
		std::abort();
	}
	web_window_test_ready();
	window->pump_until_quit();
	return 0;
}

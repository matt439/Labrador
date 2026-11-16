#include "engine/app/window.h"
#include "engine/app/motion_preference.h"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
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

EM_JS_DEPS(web_window_fullscreen_probe, "$JSEvents");
EM_JS(void, web_window_test_queue_other_activation, (), {
	Module.labradorOtherActivationCalls = 0;
	JSEvents.deferCall(() => { ++Module.labradorOtherActivationCalls; }, 1, []);
});

EM_JS(void, web_window_test_remember_host_metadata, (), {
	Module.labradorWindowHostMetadata = {
		title: document.title,
		icons: Array.from(document.querySelectorAll('link[rel~="icon"]'),
			(node) => node.outerHTML),
	};
});

EM_JS(void, web_window_cursor_fixtures_begin, (), {
	const style = Module.canvas.style;
	Module.labradorCursorFixtures = [];
	Module.labradorCursorFixtureErrors = [];
	Module.labradorCursorFixtureQuits = 0;
	Module.labradorCursorFixtureSaved = {
		value: style.getPropertyValue('cursor'), priority: style.getPropertyPriority('cursor'),
		error: Module.onError, quit: Module.onQuit,
	};
	Module.onError = (message) => Module.labradorCursorFixtureErrors.push(String(message));
	Module.onQuit = () => ++Module.labradorCursorFixtureQuits;
});

EM_JS(void, web_window_cursor_fixture_baseline, (), {
	const saved = Module.labradorCursorFixtureSaved;
	if (saved.value) Module.canvas.style.setProperty('cursor', saved.value, saved.priority);
	else Module.canvas.style.removeProperty('cursor');
});

EM_JS(void, web_window_cursor_fixture_edit, (const char* value, const char* priority), {
	Module.canvas.style.setProperty('cursor', UTF8ToString(value), UTF8ToString(priority));
});

EM_JS(void, web_window_cursor_fixture_record, (const char* label, int shape, int status), {
	const style = Module.canvas.style;
	Module.labradorCursorFixtures.push({ label: UTF8ToString(label), shape, status,
		value: style.getPropertyValue('cursor'), priority: style.getPropertyPriority('cursor') });
});

EM_JS(void, web_window_cursor_fixture_key, (), {
	// The callback failure fixture needs no trusted gesture, focus change or
	// application loop. The live Window's registered DOM listener receives it.
	Module.canvas.dispatchEvent(new KeyboardEvent('keydown', {
		code: 'ArrowLeft', key: 'ArrowLeft', bubbles: false, cancelable: true,
	}));
});

EM_JS(void, web_window_cursor_fixtures_end, (), {
	const saved = Module.labradorCursorFixtureSaved;
	if (saved.value) Module.canvas.style.setProperty('cursor', saved.value, saved.priority);
	else Module.canvas.style.removeProperty('cursor');
	Module.onError = saved.error;
	Module.onQuit = saved.quit;
	delete Module.labradorCursorFixtureSaved;
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
		explicit Recorder(bool quiet = false, bool throw_on_size = false) :
			quiet_(quiet), throw_on_size_(throw_on_size)
		{
		}

		int size_reports() const { return this->size_reports_; }

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
		void on_keyboard_gained() override
		{
			this->mouse_.set_focused(true);
			this->note("keyboard_gained");
		}
		void on_keyboard_lost() override
		{
			this->mouse_.set_focused(false);
			this->note("keyboard_lost");
		}

		// The runner owns this frame boundary, so real browser input can be
		// delivered entirely between two Mouse polls, independent of rAF.
		int poll_mouse()
		{
			this->mouse_.poll();
			return (this->mouse_.held(labrador::MouseButton::left) ? 1 : 0) |
				(this->mouse_.pressed(labrador::MouseButton::left) ? 2 : 0) |
				(this->mouse_.released(labrador::MouseButton::left) ? 4 : 0);
		}

		void on_fullscreen_changed(bool fullscreen) override
		{
			this->note(fullscreen ? "fullscreen 1" : "fullscreen 0");
		}

		void on_closed() override { this->note("closed"); }

		void on_window_moved() const override { this->note("moved"); }

		void on_window_size_changed(int width, int height) override
		{
			++this->size_reports_;
			if (this->throw_on_size_)
			{
				throw std::runtime_error("web cursor construction failure");
			}
			this->width_ = width;
			this->height_ = height;
			this->note("size " + std::to_string(width) + " " +
				std::to_string(height));
		}

		void on_key_down(labrador::Key key) const override
		{
			if (this->throw_on_key_)
			{
				throw std::runtime_error("web window test failure");
			}
			this->note("key_down " + std::string(labrador::key_name(key)));
		}

		// The next key down throws, as an owner's exception would, so the
		// window's failure path can be shown to release like a quit.
		void throw_on_key() { this->throw_on_key_ = true; }

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
			this->mouse_.on_button_down(button);
			this->note(std::string("button_down ") + button_name(button));
		}

		void on_mouse_button_up(labrador::MouseButton button) const override
		{
			this->mouse_.on_button_up(button);
			this->note(std::string("button_up ") + button_name(button));
		}

		void on_mouse_capture_lost() const override
		{
			this->mouse_.cancel_buttons();
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
			if (this->quiet_)
			{
				return;
			}
			record(entry);
			this->ticked_ = false;
		}

		int width_ = 0;
		int height_ = 0;
		mutable bool ticked_ = false;
		bool throw_on_key_ = false;
		mutable labrador::Mouse mouse_;
		bool quiet_ = false;
		bool throw_on_size_ = false;
		int size_reports_ = 0;
	};

	// Static, because in a browser pump_until_quit unwinds main's stack
	// before the first frame (window.h), and both are called from frames.
	Recorder recorder;
	std::unique_ptr<labrador::Window> window;

	labrador::WindowOptions window_options()
	{
		labrador::WindowOptions options;
		options.window_title = L"Native option must not replace the host title";
		options.window_icon_resource = L"MISSING_NATIVE_ICON_IGNORED_BY_BROWSER";
		// Smaller than the box the page gives the canvas, so the first report
		// is visibly the measured box rather than the size asked for.
		options.client_size = labrador::Vector2I(320, 200);
		return options;
	}

	int cursor_index(const labrador::Window& observed)
	{
		const std::optional<labrador::CursorShape> selected = observed.cursor_shape();
		return selected.has_value() ? static_cast<int>(*selected) : -1;
	}

	int closed_cursor_status(const labrador::Window& observed)
	{
		const std::optional<labrador::CursorShape> selected = observed.cursor_shape();
		observed.set_cursor_visible(false);
		observed.set_cursor_confined(true);
		observed.set_cursor_shape(labrador::CursorShape::move);
		observed.set_cursor_shape(std::nullopt);
		try
		{
			observed.set_cursor_shape(static_cast<labrador::CursorShape>(99));
		}
		catch (const std::invalid_argument&)
		{
			return 0;
		}
		return observed.handle() == nullptr && observed.cursor_shape() == selected ? 1 : 0;
	}

	// No fixture pumps a loop. Each destroys its Window before the main page
	// owns the canvas, including any pending on_closed notification.
	void cursor_lifecycle_fixtures()
	{
		web_window_cursor_fixtures_begin();
		labrador::WindowOptions options = window_options();
		options.cursor_shape = labrador::CursorShape::hand;
		Recorder quiet(true);
		{
			labrador::Window scoped(options, &quiet);
			web_window_cursor_fixture_record("destructor active", cursor_index(scoped), 1);
		}
		web_window_cursor_fixture_record("destructor restored", -1, 1);
		{
			labrador::Window scoped(options, &quiet);
			web_window_cursor_fixture_edit("zoom-in", "");
		}
		web_window_cursor_fixture_record("destructor keeps page edit", -1, 1);
		web_window_cursor_fixture_baseline();
		{
			labrador::Window scoped(options, &quiet);
			scoped.close();
			web_window_cursor_fixture_record("close restored", cursor_index(scoped), closed_cursor_status(scoped));
		}
		{
			labrador::Window scoped(options, &quiet);
			web_window_cursor_fixture_edit("pointer", "");
			scoped.close();
			web_window_cursor_fixture_record("close keeps page priority edit", cursor_index(scoped), closed_cursor_status(scoped));
		}
		web_window_cursor_fixture_baseline();
		quiet.throw_on_key();
		{
			labrador::Window scoped(options, &quiet);
			web_window_cursor_fixture_key();
			web_window_cursor_fixture_record("failure restored", cursor_index(scoped), closed_cursor_status(scoped));
		}
		{
			labrador::Window scoped(options, &quiet);
			web_window_cursor_fixture_edit("wait", "");
			web_window_cursor_fixture_key();
			web_window_cursor_fixture_record("failure keeps page edit", cursor_index(scoped), closed_cursor_status(scoped));
		}
		web_window_cursor_fixture_baseline();
		Recorder constructor_notify(true, true);
		int constructor_failed = 0;
		try
		{
			labrador::Window scoped(options, &constructor_notify);
		}
		catch (const std::runtime_error& error)
		{
			constructor_failed = std::string(error.what()) == "web cursor construction failure" &&
				constructor_notify.size_reports() == 1 ? 1 : 0;
		}
		web_window_cursor_fixture_record("constructor failure", -1, constructor_failed);
		options.cursor_shape = static_cast<labrador::CursorShape>(99);
		int invalid_failed = 0;
		try
		{
			labrador::Window scoped(options, &quiet);
		}
		catch (const std::invalid_argument&)
		{
			invalid_failed = 1;
		}
		web_window_cursor_fixture_record("invalid constructor", -1, invalid_failed);
		web_window_cursor_fixtures_end();
	}
}

extern "C" EMSCRIPTEN_KEEPALIVE int web_window_test_set_cursor(int shape)
{
	try
	{
		window->set_cursor_shape(shape == -1 ? std::nullopt :
			std::optional<labrador::CursorShape>(static_cast<labrador::CursorShape>(shape)));
		return 0;
	}
	catch (const std::invalid_argument&)
	{
		return 1;
	}
}

extern "C" EMSCRIPTEN_KEEPALIVE int web_window_test_cursor()
{
	return cursor_index(*window);
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_set_cursor_visible(int visible)
{
	window->set_cursor_visible(visible != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_set_cursor_confined(int confined)
{
	window->set_cursor_confined(confined != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE int web_window_test_poll_mouse()
{
	return recorder.poll_mouse();
}

extern "C" EMSCRIPTEN_KEEPALIVE int web_window_test_motion_preference()
{
	return labrador::prefers_reduced_motion() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_enter_fullscreen()
{
	window->enter_fullscreen();
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_leave_fullscreen()
{
	window->leave_fullscreen(labrador::Vector2I(320, 200));
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_queue_other_activation_call()
{
	web_window_test_queue_other_activation();
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_reopen()
{
	window = std::make_unique<labrador::Window>(window_options(), &recorder);
	window->pump_until_quit();
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_set_title()
{
	if (window)
	{
		window->set_title(L"Native runtime title must not replace the host title");
		window->set_title(std::wstring(L"ignored\0suffix", 14));
	}
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

// A second window on the same canvas, after the first has closed, whose owner
// throws on the next key: the failure path, which closes it as quit does.
extern "C" EMSCRIPTEN_KEEPALIVE void web_window_test_reopen_throwing()
{
	recorder.throw_on_key();
	window = std::make_unique<labrador::Window>(window_options(), &recorder);
}

int main()
{
	try
	{
		web_window_test_remember_host_metadata();

		cursor_lifecycle_fixtures();
		window = std::make_unique<labrador::Window>(window_options(), &recorder);
		const labrador::WindowDisplayInfo display = window->display_info();
		if (!display.monitor_id.empty() || display.nominal_refresh_hz.has_value())
		{
			throw std::runtime_error("A browser window invented nominal monitor metadata");
		}
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

#include "engine/app/window.h"
#include "engine/app/web/dom_events.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <tuple>

// THE PAGE'S HALF, which is the only JavaScript in the engine. Each function
// is one question about the canvas or one call to the page, and everything
// else goes through Emscripten's html5 API, which names an element by a
// string. "!canvas" is that string for the canvas the page handed over as
// Module.canvas: the page decides which canvas, and no selector is compiled
// in.
//
// Global rather than in labrador::, because EM_JS declares a C function, and
// prefixed for the same reason.
EM_JS_DEPS(labrador_window, "$specialHTMLTargets,$UTF8ToString");

EM_JS(int, labrador_adopt_canvas, (), {
	if (!Module.canvas) {
		return 0;
	}
	specialHTMLTargets["!canvas"] = Module.canvas;
	return 1;
});

EM_JS(int, labrador_canvas_has_focus, (), {
	return document.activeElement === specialHTMLTargets["!canvas"] ? 1 : 0;
});

EM_JS(double, labrador_canvas_left, (), {
	return specialHTMLTargets["!canvas"].getBoundingClientRect().left;
});

EM_JS(double, labrador_canvas_top, (), {
	return specialHTMLTargets["!canvas"].getBoundingClientRect().top;
});

EM_JS(void, labrador_tell_page_quit, (), {
	if (typeof Module.onQuit === "function") {
		Module.onQuit();
	}
});

EM_JS(void, labrador_tell_page_error, (const char* message), {
	if (typeof Module.onError === "function") {
		Module.onError(UTF8ToString(message));
	}
});

namespace labrador
{
	namespace
	{
		constexpr char canvas_target[] = "!canvas";
	}

	// The page's canvas. Every html5 callback carries this object as its
	// user data, from listen(true) until stop() takes them all away.
	struct Window::Impl
	{
		Impl(const WindowOptions& options, WindowNotify* window_notify);
		~Impl();

		Impl(const Impl&) = delete;
		Impl& operator=(const Impl&) = delete;

		// Registers every callback, or with `on` false removes them all.
		void listen(bool on);

		// THE DRAWING BUFFER FOLLOWS THE CANVAS'S BOX, in device pixels: its
		// CSS size times devicePixelRatio, and never below the minimum the
		// options name. The page lays the canvas out, so the size is measured
		// rather than chosen, once a frame before the tick - which also
		// catches the two changes no layout event announces, a zoom and a
		// move to a display of another density. A canvas with no box yet,
		// which is what display:none gives, keeps the size it has.
		//
		// The page must size the canvas with CSS. One left to its default
		// layout is as big as its drawing buffer, so a buffer that follows
		// it at any ratio but one grows without end.
		//
		// Returns whether the size changed, and tells nobody.
		bool measure();

		// Measures, and reports whatever size the canvas has now.
		void report_size();

		// A mouse event's position as client pixels of the drawing buffer.
		mattmath::Vector2I client_position(
			const EmscriptenMouseEvent& event) const;
		bool inside(const mattmath::Vector2I& position) const;

		// Cancels the loop and stops listening. Idempotent.
		void stop();

		// What an exception that reached the edge of the engine's code
		// becomes. On Windows it would come out of pump_until_quit and
		// main's catch would show it; here there is no caller left on the
		// stack to throw to, so it stops everything and goes to the page.
		void fail(const char* message);

		// Runs `handler` unless the window has stopped, and turns anything it
		// throws into fail(). Returns whether it ran to the end.
		template <typename Handler>
		bool guarded(Handler&& handler)
		{
			if (this->closed)
			{
				return false;
			}
			try
			{
				handler();
				return true;
			}
			catch (const std::exception& error)
			{
				this->fail(error.what());
			}
			catch (...)
			{
				this->fail("An exception that is not a std::exception "
					"reached the window.");
			}
			return false;
		}

		// The frame, and the html5 callbacks, each handed this object as its
		// user data. Members so that they can name it; nothing reaches them
		// but the pointers pump_until_quit and listen() hand over.
		static void frame(void* user_data);
		static bool on_key(int event_type,
			const EmscriptenKeyboardEvent* event, void* user_data);
		static bool on_mouse_down(int event_type,
			const EmscriptenMouseEvent* event, void* user_data);
		static bool on_mouse_move(int event_type,
			const EmscriptenMouseEvent* event, void* user_data);
		static bool on_mouse_up(int event_type,
			const EmscriptenMouseEvent* event, void* user_data);
		static bool on_wheel(int event_type,
			const EmscriptenWheelEvent* event, void* user_data);
		static bool on_focus(int event_type,
			const EmscriptenFocusEvent* event, void* user_data);
		static bool on_visibility(int event_type,
			const EmscriptenVisibilityChangeEvent* event, void* user_data);

		WindowNotify* notify = nullptr;
		int min_width = 0;
		int min_height = 0;

		// The drawing buffer and the box it fills, kept so that a mouse
		// position in CSS pixels can be scaled into the buffer's.
		int buffer_width = 0;
		int buffer_height = 0;
		double css_width = 0.0;
		double css_height = 0.0;

		// What the last focus and visibility events said, so a repeat says
		// nothing.
		bool focused = false;
		bool hidden = false;

		// The analogue of the Win32 window's capture count. A press that
		// starts on the canvas is followed off it, and its release is heard
		// wherever it happens, because the move and release listeners are on
		// the page's window rather than the canvas. Without that, a button let
		// go off the canvas would stay held in the game until it was pressed
		// and released again.
		int held_buttons = 0;

		// Set by pump_until_quit, cleared by stop().
		bool looping = false;

		// Set by stop(), and never cleared: a closed window does not reopen.
		bool closed = false;
	};

	Window::Impl::Impl(const WindowOptions& options,
		WindowNotify* window_notify) :
		notify(window_notify),
		min_width(options.min_window_width),
		min_height(options.min_window_height)
	{
		if (labrador_adopt_canvas() == 0)
		{
			throw std::runtime_error("There is no canvas to draw on: the page "
				"has to hand one to the module as Module.canvas.");
		}

		// The size asked for, until the page's layout says otherwise. It
		// stands only while the canvas has no box of its own.
		this->buffer_width = std::max(options.client_size.x, this->min_width);
		this->buffer_height = std::max(options.client_size.y, this->min_height);
		emscripten_set_canvas_element_size(canvas_target, this->buffer_width,
			this->buffer_height);

		this->focused = labrador_canvas_has_focus() != 0;
		EmscriptenVisibilityChangeEvent visibility;
		if (emscripten_get_visibility_status(&visibility) ==
			EMSCRIPTEN_RESULT_SUCCESS)
		{
			this->hidden = visibility.hidden;
		}

		// The report window.h says the caller depends on, made before any
		// callback is registered: if the owner throws from it, there is
		// nothing for a destructor that will not run to take away.
		this->report_size();

		this->listen(true);

		// Deferred to the first key or button event, which is the only place a
		// browser grants it. A page that may not go full screen at all - an
		// iframe without permission - refuses, and the game plays in the page.
		if (options.fullscreen)
		{
			std::ignore = emscripten_request_fullscreen(canvas_target, true);
		}
	}

	Window::Impl::~Impl()
	{
		// A loop still running here means the owner is being destroyed while
		// the browser goes on calling it, and the one way that happens is an
		// owner that lived on the stack pump_until_quit unwound. That is a
		// broken contract rather than an exit, so it is said, and the loop is
		// stopped before it calls into what is left.
		if (this->looping && !this->closed)
		{
			static constexpr char message[] =
				"The window was destroyed while the browser was still running "
				"its frame loop. In a browser the Application must outlive "
				"main: see Application::run.";
			std::fprintf(stderr, "%s\n", message);
			this->stop();
			labrador_tell_page_error(message);
			return;
		}
		this->stop();
	}

	void Window::Impl::listen(bool on)
	{
		// Removing is registering a null callback, which is how the html5 API
		// spells it.
		void* const user_data = on ? this : nullptr;

		const EMSCRIPTEN_RESULT results[] = {
			emscripten_set_keydown_callback(canvas_target, user_data, false,
				on ? on_key : nullptr),
			emscripten_set_keyup_callback(canvas_target, user_data, false,
				on ? on_key : nullptr),
			emscripten_set_mousedown_callback(canvas_target, user_data, false,
				on ? on_mouse_down : nullptr),
			emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,
				user_data, false, on ? on_mouse_move : nullptr),
			emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,
				user_data, false, on ? on_mouse_up : nullptr),
			emscripten_set_wheel_callback(canvas_target, user_data, false,
				on ? on_wheel : nullptr),
			emscripten_set_focus_callback(canvas_target, user_data, false,
				on ? on_focus : nullptr),
			emscripten_set_blur_callback(canvas_target, user_data, false,
				on ? on_focus : nullptr),
			emscripten_set_visibilitychange_callback(user_data, false,
				on ? on_visibility : nullptr),
		};

		if (!on)
		{
			return;
		}
		for (const EMSCRIPTEN_RESULT result : results)
		{
			if (result != EMSCRIPTEN_RESULT_SUCCESS)
			{
				this->listen(false);
				throw std::runtime_error(
					"Could not listen for input on the canvas.");
			}
		}
	}

	bool Window::Impl::measure()
	{
		double width = 0.0;
		double height = 0.0;
		if (emscripten_get_element_css_size(canvas_target, &width, &height) !=
			EMSCRIPTEN_RESULT_SUCCESS || width <= 0.0 || height <= 0.0)
		{
			return false;
		}
		this->css_width = width;
		this->css_height = height;

		const double ratio = emscripten_get_device_pixel_ratio();
		const int device_width = std::max(this->min_width,
			static_cast<int>(std::lround(width * ratio)));
		const int device_height = std::max(this->min_height,
			static_cast<int>(std::lround(height * ratio)));

		if (device_width == this->buffer_width &&
			device_height == this->buffer_height)
		{
			return false;
		}

		this->buffer_width = device_width;
		this->buffer_height = device_height;
		emscripten_set_canvas_element_size(canvas_target, device_width,
			device_height);
		return true;
	}

	void Window::Impl::report_size()
	{
		this->measure();
		this->notify->on_window_size_changed(this->buffer_width,
			this->buffer_height);
	}

	mattmath::Vector2I Window::Impl::client_position(
		const EmscriptenMouseEvent& event) const
	{
		// Measured from the canvas's corner whichever element the listener
		// is on - two of them are on the page's window, which reports from
		// the page's corner - and scaled from CSS pixels into the buffer's.
		const double scale_x = this->css_width > 0.0 ?
			this->buffer_width / this->css_width : 1.0;
		const double scale_y = this->css_height > 0.0 ?
			this->buffer_height / this->css_height : 1.0;
		const double x = (event.clientX - labrador_canvas_left()) * scale_x;
		const double y = (event.clientY - labrador_canvas_top()) * scale_y;

		// Floored, so a position just left of or above the canvas is
		// negative rather than rounded onto its first column.
		return mattmath::Vector2I(static_cast<int>(std::floor(x)),
			static_cast<int>(std::floor(y)));
	}

	bool Window::Impl::inside(const mattmath::Vector2I& position) const
	{
		return position.x >= 0 && position.y >= 0 &&
			position.x < this->buffer_width && position.y < this->buffer_height;
	}

	void Window::Impl::stop()
	{
		if (this->closed)
		{
			return;
		}
		this->closed = true;
		if (this->looping)
		{
			emscripten_cancel_main_loop();
			this->looping = false;
		}
		this->listen(false);
	}

	void Window::Impl::fail(const char* message)
	{
		std::fprintf(stderr, "%s\n", message);
		this->stop();
		labrador_tell_page_error(message);
	}

	void Window::Impl::frame(void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		self->guarded([self]()
			{
				self->measure();
				self->notify->tick();
			});
	}

	bool Window::Impl::on_key(int event_type,
		const EmscriptenKeyboardEvent* event, void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		const Key key = key_from_code(event->code);

		// Key repeat needs no filtering, for the reason the Win32 window
		// gives: a repeat sets a bit that is already set, and the edges come
		// from polled frames. Its text is still text, as WM_CHAR's is.
		const bool handled = self->guarded([&]()
			{
				if (event_type == EMSCRIPTEN_EVENT_KEYDOWN)
				{
					self->notify->on_key_down(key);
					const char32_t text = text_from_key(event->key,
						event->ctrlKey, event->altKey, event->metaKey);
					if (text != 0)
					{
						self->notify->on_text(text);
					}
				}
				else
				{
					self->notify->on_key_up(key);
				}
			});

		// True asks Emscripten to call preventDefault.
		return handled && !browser_keeps_key(key, event->ctrlKey,
			event->altKey, event->metaKey);
	}

	bool Window::Impl::on_mouse_down(int, const EmscriptenMouseEvent* event,
		void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		const MouseButton button = mouse_button_from_dom(event->button);
		if (button == MouseButton::none)
		{
			return false;
		}

		self->guarded([&]()
			{
				const mattmath::Vector2I position = self->client_position(*event);
				self->notify->on_mouse_move(position.x, position.y);
				++self->held_buttons;
				self->notify->on_mouse_button_down(button);
			});

		// The default is left alone: a press on a focusable canvas is what
		// gives it the keyboard.
		return false;
	}

	bool Window::Impl::on_mouse_move(int, const EmscriptenMouseEvent* event,
		void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		self->guarded([&]()
			{
				const mattmath::Vector2I position = self->client_position(*event);
				if (self->held_buttons > 0 || self->inside(position))
				{
					self->notify->on_mouse_move(position.x, position.y);
				}
			});
		return false;
	}

	bool Window::Impl::on_mouse_up(int, const EmscriptenMouseEvent* event,
		void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		const MouseButton button = mouse_button_from_dom(event->button);
		if (button == MouseButton::none)
		{
			return false;
		}

		self->guarded([&]()
			{
				if (self->held_buttons > 0)
				{
					--self->held_buttons;
				}
				else if (!self->inside(self->client_position(*event)))
				{
					// A release over some other part of the page, of a press
					// that never reached the canvas.
					return;
				}
				self->notify->on_mouse_button_up(button);
			});
		return false;
	}

	// ONLY WHILE THE CANVAS HAS THE KEYBOARD. A visitor scrolling down the
	// page past the game has not asked to play it, and taking the wheel from
	// them would trap the page under the cursor. Once they have clicked in,
	// the wheel is the game's, and the page does not scroll under it.
	bool Window::Impl::on_wheel(int, const EmscriptenWheelEvent* event,
		void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		if (!self->focused)
		{
			return false;
		}

		// The vertical wheel negated, because the DOM signs it as the page
		// scrolling and WM_MOUSEWHEEL as the wheel turning (dom_events.h).
		return self->guarded([&]()
			{
				if (event->deltaY != 0.0)
				{
					self->notify->on_mouse_wheel(
						-wheel_notches(event->deltaY, event->deltaMode));
				}
				if (event->deltaX != 0.0)
				{
					self->notify->on_mouse_wheel_horizontal(
						wheel_notches(event->deltaX, event->deltaMode));
				}
			});
	}

	// The canvas gaining and losing the keyboard, which is what the foreground
	// is to a game in a page: on_deactivated releases every held key, exactly
	// as alt-tab does.
	bool Window::Impl::on_focus(int event_type, const EmscriptenFocusEvent*,
		void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		const bool focused = event_type == EMSCRIPTEN_EVENT_FOCUS;
		if (focused == self->focused)
		{
			return false;
		}
		self->focused = focused;

		self->guarded([&]()
			{
				if (focused)
				{
					self->notify->on_activated();
					return;
				}

				// Losing the keyboard mid-drag is a lost capture, as
				// WM_CAPTURECHANGED is on Windows: the release will be
				// delivered, if at all, to whatever has the page now.
				if (self->held_buttons > 0)
				{
					self->held_buttons = 0;
					self->notify->on_mouse_capture_lost();
				}
				self->notify->on_deactivated();
			});
		return false;
	}

	// A tab in the background gets no animation frames, so it is suspended in
	// the sense the Win32 window's minimise is: the clock is reset on the way
	// back rather than paying off the time away.
	bool Window::Impl::on_visibility(int,
		const EmscriptenVisibilityChangeEvent* event, void* user_data)
	{
		Impl* const self = static_cast<Impl*>(user_data);
		const bool hidden = event->hidden;
		if (hidden == self->hidden)
		{
			return false;
		}
		self->hidden = hidden;

		self->guarded([&]()
			{
				if (hidden)
				{
					self->notify->on_suspending();
				}
				else
				{
					self->notify->on_resuming();
				}
			});
		return false;
	}

	Window::Window(const WindowOptions& options, WindowNotify* notify) :
		impl_(std::make_unique<Impl>(options, notify))
	{
	}

	Window::~Window() = default;

	void* Window::handle() const
	{
		return this->impl_->closed ? nullptr :
			const_cast<char*>(canvas_target);
	}

	int Window::exit_code() const
	{
		return 0;
	}

	void Window::pump_until_quit()
	{
		// Closed before the loop began - a first state that quit from its
		// init(), say - is a loop with nothing to run, which is what the
		// Win32 pump does with the WM_QUIT already waiting for it.
		if (this->impl_->closed)
		{
			return;
		}

		// Zero frames a second is requestAnimationFrame, so the browser paces
		// the loop at the display's rate. True is "simulate an infinite loop":
		// this call throws to the page instead of returning, which is the
		// unwinding window.h warns about.
		this->impl_->looping = true;
		emscripten_set_main_loop_arg(Impl::frame, this->impl_.get(), 0, true);
	}

	void Window::close() const
	{
		if (this->impl_->closed)
		{
			return;
		}
		this->impl_->stop();
		labrador_tell_page_quit();
	}

	void Window::resize_client(const mattmath::Vector2I&) const
	{
		this->impl_->report_size();
	}

	void Window::enter_fullscreen() const
	{
		std::ignore = emscripten_request_fullscreen(canvas_target, true);
	}

	void Window::leave_fullscreen(const mattmath::Vector2I&) const
	{
		// The box changes when the browser has finished leaving, which is
		// after this returns, and the next frame's measure() reports it then.
		// The report here is of the size the canvas has now, which keeps the
		// caller from holding the size it asked for in the meantime.
		std::ignore = emscripten_exit_fullscreen();
		this->impl_->report_size();
	}
}

#include "engine/app/application.h"
#include "engine/app/content_root.h"
#include "engine/assets/asset_manifest_loader.h"
#include "engine/render/resource_factory.h"
#include "engine/math/vector2f.h"

#if defined(_WIN32)
#include <DirectXMath.h>
#include <objbase.h>
#endif

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

namespace labrador
{
	int default_thread_count()
	{
		const unsigned int reported = std::thread::hardware_concurrency();
		// Zero means the implementation could not tell. One is the honest
		// answer to "I do not know" - guessing high is the mistake this
		// function exists to stop making.
		return reported == 0u ? 1 : static_cast<int>(reported);
	}

	void ApplicationOptions::validate() const
	{
		const auto require = [](bool condition, const char* message)
			{
				if (!condition)
				{
					throw std::invalid_argument(message);
				}
			};

		require(this->target_fps > 0,
			"ApplicationOptions::target_fps must be greater than zero.");
		require(StepTimer::SecondsToTicks(1.0 / this->target_fps) > 0,
			"ApplicationOptions::target_fps exceeds the timer resolution.");
		require(this->min_threads >= 1,
			"ApplicationOptions::min_threads must be at least 1.");
		require(this->max_threads >= this->min_threads,
			"ApplicationOptions::max_threads must be at least min_threads.");
		require(this->view_capacity >= 1,
			"ApplicationOptions::view_capacity must be at least 1.");
		require(this->min_window_width > 0,
			"ApplicationOptions::min_window_width must be greater than zero.");
		require(this->min_window_height > 0,
			"ApplicationOptions::min_window_height must be greater than zero.");
		require(!this->window_class_name.empty(),
			"ApplicationOptions::window_class_name must not be empty.");
	}

	Application::Application(ApplicationOptions options) :
		options_(std::move(options))
	{
		// Before anything is built out of them, and before the window exists,
		// so a bad number is a message rather than a first-frame crash.
		this->options_.validate();

		this->renderer_ = std::make_unique<Renderer>();
		this->renderer_->set_device_notify(this);
	}

	Application::~Application()
	{
		// FIRST, BEFORE ANYTHING BELOW IS TOUCHED AND BEFORE ANY MEMBER IS
		// DESTROYED. The live states are frames_ in the StateContext base, and
		// [class.dtor]/8 destroys members before bases - so without this line
		// every service declared in the header has already gone by the time
		// ~StateContext destroys the states that point at them, on every exit
		// path, since run() returns the instant WM_QUIT arrives with the stack
		// still full. Reordering the base list cannot fix it. See
		// StateContext::clear.
		//
		// CoUninitialize below is the same hazard in the destructor's own body
		// rather than in its member list, and this line is what puts the states
		// in front of it too.
		this->clear();

		if (this->audio_device_)
		{
			this->audio_device_->suspend();
		}
#if defined(_WIN32)
		if (this->com_initialized_)
		{
			CoUninitialize();
		}
#endif
	}

	void Application::initialize()
	{
		// Two duties of a Windows shell that a browser has no counterpart
		// for: DirectXMath's check that this CPU has the instructions it was
		// compiled against, and the COM apartment the Windows audio and image
		// APIs are called from.
#if defined(_WIN32)
		if (!DirectX::XMVerifyCPUSupport())
		{
			throw std::runtime_error(
				"This CPU does not support the instruction set the renderer needs.");
		}

		const HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
		if (FAILED(hr))
		{
			throw std::runtime_error("CoInitializeEx failed.");
		}
		this->com_initialized_ = true;
#endif

		this->resolution_manager_ = std::make_unique<ResolutionManager>();
		this->resolution_manager_->set_resolution(this->options_.resolution);

		this->create_window();

		// Which audio API this opens, and whether it opens one at all, is
		// engine/audio/<backend>/'s business and is chosen in CMake, debug flags
		// included.
		this->audio_device_ = std::make_unique<AudioDevice>();

		const Vector2I size =
			this->resolution_manager_->resolution_ivec();

		// The layout's number, not the pool's. A thread count here builds one
		// deferred context and one dynamic vertex buffer per thread the machine
		// might ever run, to draw a frame that has four panes at the very most -
		// see ApplicationOptions::view_capacity.
		this->renderer_->create_device(this->window_->handle(), size.x, size.y,
			this->options_.view_capacity);

		this->create_services();

		this->gamepad_reader_ = std::make_unique<GamepadReader>();
		this->gamepads_ = std::make_unique<Gamepads>(this->gamepad_reader_.get());

		// No reader to give either of these: the window feeds them. They exist
		// from here on so that a message arriving before the first frame has
		// somewhere to land - WM_MOUSEMOVE turns up the moment the cursor is
		// over a window that has only just been shown.
		this->keyboard_ = std::make_unique<Keyboard>();
		this->mouse_ = std::make_unique<Mouse>();
		this->touch_ = std::make_unique<Touch>();

		// The window is already up by the time services are built, and a
		// window that was given the foreground on creation gets no
		// WM_ACTIVATEAPP to say so - it was already active when the handler
		// that would have heard it did not exist. Without this the first key
		// press after launch lands on an unfocused device and is discarded,
		// and the player's first input in the process is silently the one that
		// does nothing.
		//
		// What a canvas that does not have the keyboard said in the window's
		// constructor is kept in has_keyboard_ and applied here, so the same
		// line covers both.
		this->set_input_focus(this->foreground_ && this->shown_ &&
			this->has_keyboard_);
		this->touch_->set_focused(this->foreground_ && this->shown_);

		this->timer_.SetFixedTimeStep(true);
		this->timer_.SetTargetElapsedSeconds(
			1.0 / static_cast<double>(this->options_.target_fps));
	}

	void Application::create_window()
	{
		WindowOptions window_options;
		window_options.window_class_name = this->options_.window_class_name;
		window_options.window_title = this->options_.window_title;
		window_options.client_size =
			this->resolution_manager_->resolution_ivec();
		window_options.fullscreen = this->options_.fullscreen;
		window_options.visible = this->options_.visible;
		window_options.cursor_visible = this->options_.cursor_visible;
		window_options.cursor_confined = this->options_.cursor_confined;
		window_options.min_window_width = this->options_.min_window_width;
		window_options.min_window_height = this->options_.min_window_height;

		// `this` is handed over in the constructor, not after it: the window's
		// first size report arrives before this call returns, and it is what
		// corrects resolution_manager_ to the client size the window really
		// got before create_device reads it back. A window created hidden on
		// Windows makes no report when windowed and reports the requested
		// size when full screen; either way resolution_manager_ keeps the
		// requested size, which is the client size it got unless that is
		// below the smallest window the system allows.
		this->window_ = std::make_unique<Window>(window_options, this);
	}

	void Application::create_services()
	{
		this->thread_pool_ = std::make_unique<ThreadPool>(
			this->options_.min_threads, this->options_.max_threads);

		this->render_resources_ = std::make_unique<RenderResources>();
		this->audio_resources_ = std::make_unique<AudioResources>();

		// The draw lists resolve handles against the table, so the renderer has
		// to be told where it is. It could not be told at create_device time:
		// the loader needs a device before it can put anything in a table.
		this->renderer_->set_resources(this->render_resources_.get());
		add_white_texture(*this->renderer_, *this->render_resources_);

		this->resource_loader_ = std::make_unique<ResourceLoader>(
			this->render_resources_.get(), this->renderer_.get(),
			this->audio_resources_.get(), this->audio_device_.get());

		this->viewport_manager_ = std::make_unique<ViewportManager>(
			this->resolution_manager_.get());

		this->partitioner_ = std::make_unique<Partitioner>();
	}

	void Application::load_manifest(const std::string& manifest_path)
	{
		// Both sentences of content_root.h: the manifest under the
		// executable's directory unless it is absolute, and every relative
		// directory inside it under the manifest's.
		const std::string path =
			resolved_under(executable_directory(), manifest_path);
		this->resource_loader_->load_manifest(
			anchored_to_source(read_asset_manifest(path.c_str())));
		this->content_loaded_ = true;
	}

	int Application::run(std::unique_ptr<State> first_state)
	{
		this->transition_to(std::move(first_state));

		this->window_->pump_until_quit();
		return this->window_->exit_code();
	}

	void Application::quit() const
	{
		this->window_->close();
	}

	void Application::set_resolution(ScreenResolution resolution)
	{
		this->options_.resolution = resolution;
		this->resolution_manager_->set_resolution(resolution);

		// The WM_SIZE this produces re-points the resolution manager at what
		// the window actually became, which is not always what was asked for -
		// a size past the monitor's comes back clamped.
		this->window_->resize_client(
			this->resolution_manager_->resolution_ivec());
	}

	void Application::set_fullscreen(bool fullscreen)
	{
		this->options_.fullscreen = fullscreen;

		if (fullscreen)
		{
			this->window_->enter_fullscreen();
		}
		else
		{
			// BACK TO THE SIZE THE GAME ASKED FOR, WHICH IS NOT THE SIZE THE
			// MANAGER HOLDS. resolution_ivec() is the live window rather than the
			// requested preset, so while full screen
			// it is the monitor - reading it here would keep the window at
			// monitor size with a caption on it. options_.resolution is the last
			// thing a game actually requested, so the manager is re-pointed at
			// it, and the WM_SIZE below then corrects it to whatever client area
			// that yields.
			this->resolution_manager_->set_resolution(this->options_.resolution);
			this->window_->leave_fullscreen(
				this->resolution_manager_->resolution_ivec());
		}
	}

	bool Application::fullscreen() const
	{
		return this->options_.fullscreen;
	}

	// Kept in the options as well as handed on, so a call before the window
	// exists is the option it would have been.
	void Application::set_cursor_visible(bool visible)
	{
		this->options_.cursor_visible = visible;
		if (this->window_)
		{
			this->window_->set_cursor_visible(visible);
		}
	}

	bool Application::cursor_visible() const
	{
		return this->options_.cursor_visible;
	}

	void Application::set_cursor_confined(bool confined)
	{
		this->options_.cursor_confined = confined;
		if (this->window_)
		{
			this->window_->set_cursor_confined(confined);
		}
	}

	bool Application::cursor_confined() const
	{
		return this->options_.cursor_confined;
	}

	void Application::reset_elapsed_time()
	{
		this->timer_.ResetElapsedTime();
		this->frame_timer_.discontinuity();
	}

	FrameTimings Application::frame_timings() const
	{
		return this->frame_timings_;
	}

	const StepPosition& Application::step_position() const
	{
		return this->step_position_;
	}

	// SIX CLOCK READS A RENDERED TICK, whatever the frame holds: one at each
	// phase boundary and none per view or per object
	// (T8). In a browser each is a call out to performance.now(), so the
	// number is fixed here rather than grown by whoever adds a phase.
	void Application::tick()
	{
		this->frame_timer_.begin(std::chrono::steady_clock::now());

		const std::uint32_t steps_before = this->timer_.GetFrameCount();
		this->timer_.Tick([&]() { this->update(); });
		this->step_position_ = labrador::step_position(
			this->timer_.GetFrameCount(), this->timer_.GetLeftOverTicks(),
			this->timer_.GetTargetElapsedTicks());

		// A state can close the window during a fixed update. Every owed
		// update still runs, but a closed window has no frame to present.
		if (this->window_->handle() == nullptr)
		{
			return;
		}

		FrameMarks marks;
		marks.updates =
			static_cast<int>(this->timer_.GetFrameCount() - steps_before);
		marks.updated = std::chrono::steady_clock::now();
		this->render(marks);
	}

	void Application::update()
	{
		// The step this update runs, already counted by the timer, so that a
		// drawable recording it here can tell in draw() whether it was stepped
		// on the latest one (step_position, application.h).
		this->step_position_ = labrador::step_position(
			this->timer_.GetFrameCount(), 0, this->timer_.GetTargetElapsedTicks());

		// Before anything reads it, and on every frame whatever is running.
		// That is what makes "down now, up last frame" true - see gamepads.h
		// for the two hand-primed edge detectors this replaced.
		//
		// Every device together, and in one place, so that a frame boundary means
		// the same thing to every device. A keyboard polled somewhere else in
		// the frame would put its edges half a frame away from the pads', and
		// a game reading both would see a stick and a key pressed on the same
		// physical frame report on different ones.
		this->gamepads_->poll();
		this->keyboard_->poll();
		this->mouse_->poll();
		this->touch_->poll();

		StateContext::update(
			static_cast<float>(this->timer_.GetElapsedSeconds()));
		this->audio_device_->update();
	}

	void Application::render(FrameMarks& marks)
	{
		// Nothing to draw before the first update has run.
		if (this->timer_.GetFrameCount() == 0)
		{
			return;
		}

		// The whole frame, and the only place it is spelt out. The state
		// declares its views and fills them; recording, executing and releasing
		// the per-view command lists is submit()'s business, in one copy,
		// behind the seam.
		this->renderer_->begin_frame();
		marks.frame_begun = std::chrono::steady_clock::now();

		this->renderer_->begin_marker(L"Render");
		StateContext::draw(*this->renderer_);
		marks.drawn = std::chrono::steady_clock::now();
		this->renderer_->submit();
		this->renderer_->end_marker();
		marks.submitted = std::chrono::steady_clock::now();

		this->renderer_->end_frame();
		marks.frame_ended = std::chrono::steady_clock::now();

		// Last, so a frame that threw anywhere above publishes nothing.
		this->frame_timer_.publish(marks, this->frame_timings_);
	}

	void Application::set_input_focus(bool focused) const
	{
		// Losing it is the load-bearing direction. A key or a button held when
		// the window goes away has its release delivered to whatever took the
		// foreground, so a device that kept the bit set would hold it until
		// the player pressed and released that key again - which is a
		// character walking left forever after an alt-tab.
		if (this->keyboard_)
		{
			this->keyboard_->set_focused(focused);
		}
		if (this->mouse_)
		{
			this->mouse_->set_focused(focused);
		}
	}

	// ALL FOUR SAY THE SAME THING TO THE STACK, and notify_activation is what
	// makes four into two: alt-tab and then minimise is two of these firing
	// with one piece of news in them, and the repeat is dropped there.
	//
	// THE STATES ARE TOLD WHILE EVERY SERVICE IS STILL BEHAVING NORMALLY. A
	// state's on_deactivated is a game's own code and will stop a looping
	// voice or pause a clock, so going away it speaks after the input devices
	// are settled and before the audio engine is suspended under it; coming
	// back it speaks last of all, into a shell that has finished becoming what
	// it is about to describe. It is the same rule that makes ~Application
	// drain the stack first: a state gets to run against live services or it
	// does not get to run.

	void Application::settle_presence()
	{
		const bool present = this->foreground_ && this->shown_;
		if (this->gamepad_reader_)
		{
			if (present)
			{
				this->gamepad_reader_->resume();
			}
			else
			{
				this->gamepad_reader_->suspend();
			}
		}
		this->set_input_focus(present && this->has_keyboard_);
		if (this->touch_)
		{
			this->touch_->set_focused(present);
		}
		this->notify_activation(present);
	}

	void Application::on_activated()
	{
		this->foreground_ = true;
		this->settle_presence();
	}

	void Application::on_deactivated()
	{
		this->foreground_ = false;
		this->settle_presence();
	}

	void Application::on_suspending()
	{
		this->shown_ = false;
		this->settle_presence();
		if (this->audio_device_)
		{
			this->audio_device_->suspend();
		}
	}

	void Application::on_resuming()
	{
		this->timer_.ResetElapsedTime();
		this->frame_timer_.discontinuity();
		this->shown_ = true;
		if (this->audio_device_)
		{
			this->audio_device_->resume();
		}
		this->settle_presence();
	}

	void Application::on_keyboard_gained()
	{
		this->has_keyboard_ = true;
		this->set_input_focus(this->foreground_ && this->shown_);
	}

	void Application::on_keyboard_lost()
	{
		this->has_keyboard_ = false;
		this->set_input_focus(false);
	}

	void Application::on_fullscreen_changed(bool fullscreen)
	{
		this->options_.fullscreen = fullscreen;
	}

	void Application::on_closed()
	{
		// The destructor's work, which in a browser nothing else does: the
		// states first, for the reason ~Application gives, then the services in
		// reverse declaration order. The window stays: it called this, it is
		// closed and it listens to nothing. The Application is left with no
		// services, which nothing reaches, since its loop and every callback
		// into it have stopped.
		this->clear();
		if (this->audio_device_)
		{
			this->audio_device_->suspend();
		}
		this->touch_.reset();
		this->mouse_.reset();
		this->keyboard_.reset();
		this->gamepads_.reset();
		this->gamepad_reader_.reset();
		this->partitioner_.reset();
		this->thread_pool_.reset();
		this->viewport_manager_.reset();
		this->resolution_manager_.reset();
		this->resource_loader_.reset();
		this->audio_resources_.reset();
		this->renderer_.reset();
		this->render_resources_.reset();
		this->audio_device_.reset();
	}

	void Application::on_window_moved() const
	{
		const Vector2F size = this->renderer_->back_buffer_size();
		std::ignore = this->renderer_->window_size_changed(
			static_cast<int>(size.x), static_cast<int>(size.y));
	}

	void Application::on_window_size_changed(int width, int height)
	{
		// THE LAYOUT SIZE FIRST. Every way a window can change size arrives
		// here - set_resolution, set_fullscreen, and a user dragging an edge,
		// which no game asked for at all - and the layout has to hear it as
		// well as the renderer. A ResolutionManager still reporting the last
		// requested preset would have ViewportManager lay out every viewport
		// and divider for that size while the game draws into a larger back
		// buffer, leaving the rest of the window cleared black.
		//
		// `width` and `height` are CLIENT pixels: WM_SIZE's lParam is the client
		// area, and the WM_EXITSIZEMOVE path uses GetClientRect. So no frame
		// arithmetic belongs here - that is create_window's problem, on the way
		// in.
		//
		// Exactly, not through the coercing overload. See
		// ResolutionManager::set_resolution_exactly: the enum-converting path
		// answers 720p for any size outside its four presets, which would make
		// this line a no-op that looks like a fix.
		this->resolution_manager_->set_resolution_exactly(
			Vector2I(width, height));

		std::ignore = this->renderer_->window_size_changed(width, height);
	}

	void Application::on_device_lost()
	{
		// Only the resources the device holds. Audio is not one, and tearing
		// down the sound banks here left every object holding a freed
		// SoundBank*. The sprite batches and the sampler states belong to the
		// renderer, which has already released them by the time this is called.
		//
		// Which resources those are is the table's business, not the shell's.
		// Naming textures and fonts here would have the shell know both what
		// kinds of resource the backend keeps and that there are exactly two of
		// them.
		this->render_resources_->release_device_resources();
	}

	void Application::on_device_restored()
	{
		// Reload the GPU-side assets into the existing RenderResources, so every
		// borrowed SpriteSheet* and SoundBank* stays valid.
		//
		// Nothing re-seats a device here. The restore hands back a different
		// one, and telling the loader so would be an ordering only this line
		// knew about. The factory reads the device off the renderer as it
		// builds, so the new device is simply what is there.
		//
		// The white texture first and whether or not a manifest was loaded,
		// because the shell put it there and no manifest names it.
		add_white_texture(*this->renderer_, *this->render_resources_);

		if (this->content_loaded_)
		{
			this->resource_loader_->reload_device_resources();
		}
	}

	Renderer* Application::renderer() const
	{
		return this->renderer_.get();
	}
	RenderResources* Application::render_resources() const
	{
		return this->render_resources_.get();
	}
	AudioResources* Application::audio_resources() const
	{
		return this->audio_resources_.get();
	}
	ResourceLoader* Application::resource_loader() const
	{
		return this->resource_loader_.get();
	}
	ResolutionManager* Application::resolution_manager() const
	{
		return this->resolution_manager_.get();
	}
	ViewportManager* Application::viewport_manager() const
	{
		return this->viewport_manager_.get();
	}
	ThreadPool* Application::thread_pool() const
	{
		return this->thread_pool_.get();
	}
	const Partitioner* Application::partitioner() const
	{
		return this->partitioner_.get();
	}
	// EIGHT FORWARDERS AND NOT ONE DECISION IN THEM, which is the point. The
	// window decided what the message was; the device decides what a frame of
	// them adds up to; the game decides what it means. Anything that looked
	// like policy appearing in this block - a key that opens a menu, a click
	// that selects - would be engine code answering a question the boundary
	// gives to the game (T1).
	//
	// The null guards are not defensive habit. Messages arrive during
	// create_window, which runs before create_services, so the first
	// WM_MOUSEMOVE of the process genuinely can land before there is a Mouse
	// to land in.
	void Application::on_key_down(Key key) const
	{
		if (this->keyboard_)
		{
			this->keyboard_->on_key_down(key);
		}
	}

	void Application::on_key_up(Key key) const
	{
		if (this->keyboard_)
		{
			this->keyboard_->on_key_up(key);
		}
	}

	void Application::on_text(char32_t codepoint) const
	{
		if (this->keyboard_)
		{
			this->keyboard_->on_text(codepoint);
		}
	}

	void Application::on_mouse_move(int x, int y) const
	{
		if (this->mouse_)
		{
			this->mouse_->on_move(Vector2I(x, y));
		}
	}

	void Application::on_mouse_button_down(MouseButton button) const
	{
		if (this->mouse_)
		{
			this->mouse_->on_button_down(button);
		}
	}

	void Application::on_mouse_button_up(MouseButton button) const
	{
		if (this->mouse_)
		{
			this->mouse_->on_button_up(button);
		}
	}

	void Application::on_mouse_capture_lost() const
	{
		if (this->mouse_)
		{
			this->mouse_->cancel_buttons();
		}
	}

	void Application::on_mouse_wheel(float notches) const
	{
		if (this->mouse_)
		{
			this->mouse_->on_wheel(notches);
		}
	}

	void Application::on_mouse_wheel_horizontal(float notches) const
	{
		if (this->mouse_)
		{
			this->mouse_->on_wheel_horizontal(notches);
		}
	}

	void Application::on_touch_down(uint32_t point, int x, int y) const
	{
		if (this->touch_)
		{
			this->touch_->on_down(point, Vector2I(x, y));
		}
	}

	void Application::on_touch_move(uint32_t point, int x, int y) const
	{
		if (this->touch_)
		{
			this->touch_->on_move(point, Vector2I(x, y));
		}
	}

	void Application::on_touch_up(uint32_t point, int x, int y) const
	{
		if (this->touch_)
		{
			this->touch_->on_up(point, Vector2I(x, y));
		}
	}

	void Application::on_touch_cancel(uint32_t point) const
	{
		if (this->touch_)
		{
			this->touch_->on_cancel(point);
		}
	}

	Keyboard* Application::keyboard() const
	{
		return this->keyboard_.get();
	}

	Mouse* Application::mouse() const
	{
		return this->mouse_.get();
	}

	Touch* Application::touch() const
	{
		return this->touch_.get();
	}

	Gamepads* Application::gamepads() const
	{
		return this->gamepads_.get();
	}
	void* Application::window() const
	{
		return this->window_->handle();
	}

}

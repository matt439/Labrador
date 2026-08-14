#include "engine/app/application.h"
#include "engine/assets/asset_manifest_loader.h"
// The one place the shell has to name the backend, and since the window moved
// out it is down to one reason: on_display_change ends in
// DeviceResources::UpdateColorSpace. That is also why the seven window
// handlers stayed on Application rather than following the Win32 into
// window.cpp - see the note above them in application.h.
#include "engine/render/d3d11/backend.h"
#include "engine/math/vector2f.h"
#include <DirectXMath.h>
#include <memory>
#include <objbase.h>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace DirectX;

namespace artattack
{
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
		require(this->min_threads >= 1,
			"ApplicationOptions::min_threads must be at least 1.");
		require(this->max_threads >= this->min_threads,
			"ApplicationOptions::max_threads must be at least min_threads.");
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

		if (this->audio_engine_)
		{
			this->audio_engine_->Suspend();
		}
		if (this->com_initialized_)
		{
			CoUninitialize();
		}
	}

	void Application::initialize(HINSTANCE instance, int show_command)
	{
		if (!XMVerifyCPUSupport())
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

		this->resolution_manager_ = std::make_unique<ResolutionManager>();
		this->resolution_manager_->set_resolution(this->options_.resolution);

		this->create_window(instance, show_command);

		AUDIO_ENGINE_FLAGS audio_flags = AudioEngine_Default;
#ifdef _DEBUG
		audio_flags |= AudioEngine_Debug;
#endif
		this->audio_engine_ = std::make_unique<AudioEngine>(audio_flags);

		const mattmath::Vector2I size =
			this->resolution_manager_->resolution_ivec();

		// max_threads is the view capacity: the widest this frame may ever fan
		// out, which is what sizes the per-view recording state.
		this->renderer_->create_device(this->window_->handle(), size.x, size.y,
			this->options_.max_threads);

		this->create_services();

		this->gamepad_reader_ = std::make_unique<GamepadReader>();
		this->gamepads_ = std::make_unique<Gamepads>(this->gamepad_reader_.get());

		// No reader to give either of these: the window feeds them. They exist
		// from here on so that a message arriving before the first frame has
		// somewhere to land - WM_MOUSEMOVE turns up the moment the cursor is
		// over a window that has only just been shown.
		this->keyboard_ = std::make_unique<Keyboard>();
		this->mouse_ = std::make_unique<Mouse>();

		// The window is already up by the time services are built, and a
		// window that was given the foreground on creation gets no
		// WM_ACTIVATEAPP to say so - it was already active when the handler
		// that would have heard it did not exist. Without this the first key
		// press after launch lands on an unfocused device and is discarded,
		// and the player's first input in the process is silently the one that
		// does nothing.
		this->keyboard_->set_focused(true);
		this->mouse_->set_focused(true);

		this->timer_.SetFixedTimeStep(true);
		this->timer_.SetTargetElapsedSeconds(
			1.0 / static_cast<double>(this->options_.target_fps));
	}

	void Application::create_window(HINSTANCE instance, int show_command)
	{
		WindowOptions window_options;
		window_options.window_class_name = this->options_.window_class_name;
		window_options.window_title = this->options_.window_title;
		window_options.client_size =
			this->resolution_manager_->resolution_ivec();
		window_options.fullscreen = this->options_.fullscreen;
		window_options.min_window_width = this->options_.min_window_width;
		window_options.min_window_height = this->options_.min_window_height;

		// `this` is handed over in the constructor, not after it: the WM_SIZE
		// ShowWindow fires arrives before this call returns, and it is what
		// corrects resolution_manager_ to the client size the window really
		// got before create_device reads it back.
		this->window_ = std::make_unique<Window>(
			instance, show_command, window_options, this);
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

		this->resource_loader_ = std::make_unique<ResourceLoader>(
			this->render_resources_.get(), this->audio_resources_.get(),
			this->renderer_->impl()->device_resources.GetD3DDevice(),
			this->audio_engine_.get());

		this->viewport_manager_ = std::make_unique<ViewportManager>(
			this->resolution_manager_.get());

		this->partitioner_ = std::make_unique<Partitioner>();
	}

	void Application::load_manifest(const std::string& manifest_path)
	{
		this->resource_loader_->load_manifest(
			read_asset_manifest(manifest_path.c_str()));
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
			// MANAGER HOLDS. resolution_ivec() used to be a synonym for the
			// requested preset and is now the live window, so while full screen
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

	void Application::reset_elapsed_time()
	{
		this->timer_.ResetElapsedTime();
	}

	void Application::tick()
	{
		this->timer_.Tick([&]() { this->update(); });
		this->render();
	}

	void Application::update()
	{
		// Before anything reads it, and on every frame whatever is running.
		// That is what makes "down now, up last frame" true - see gamepads.h
		// for the two hand-primed edge detectors this replaced.
		//
		// All three together, and in one place, so that a frame boundary means
		// the same thing to every device. A keyboard polled somewhere else in
		// the frame would put its edges half a frame away from the pads', and
		// a game reading both would see a stick and a key pressed on the same
		// physical frame report on different ones.
		this->gamepads_->poll();
		this->keyboard_->poll();
		this->mouse_->poll();

		StateContext::update(
			static_cast<float>(this->timer_.GetElapsedSeconds()));
		std::ignore = this->audio_engine_->Update();
	}

	void Application::render()
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

		this->renderer_->begin_marker(L"Render");
		StateContext::draw(*this->renderer_);
		this->renderer_->submit();
		this->renderer_->end_marker();

		this->renderer_->end_frame();
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

	void Application::on_activated()
	{
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->resume();
		}
		this->set_input_focus(true);
		this->notify_activation(true);
	}

	void Application::on_deactivated()
	{
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->suspend();
		}
		this->set_input_focus(false);
		this->notify_activation(false);
	}

	void Application::on_suspending()
	{
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->suspend();
		}
		this->set_input_focus(false);
		this->notify_activation(false);
		if (this->audio_engine_)
		{
			this->audio_engine_->Suspend();
		}
	}

	void Application::on_resuming()
	{
		this->timer_.ResetElapsedTime();
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->resume();
		}
		this->set_input_focus(true);
		if (this->audio_engine_)
		{
			this->audio_engine_->Resume();
		}
		this->notify_activation(true);
	}

	void Application::on_window_moved() const
	{
		const mattmath::Vector2F size = this->renderer_->back_buffer_size();
		std::ignore = this->renderer_->window_size_changed(
			static_cast<int>(size.x), static_cast<int>(size.y));
	}

	void Application::on_display_change() const
	{
		this->renderer_->impl()->device_resources.UpdateColorSpace();
	}

	void Application::on_window_size_changed(int width, int height)
	{
		// THE LAYOUT SIZE FIRST, AND THIS LINE IS THE WHOLE FIX. Every way a
		// window can change size arrives here - set_resolution, set_fullscreen,
		// and a user dragging an edge, which no game asked for at all - and
		// until now the only thing told about it was the renderer. The back
		// buffer became 2560x1440 while ResolutionManager went on reporting the
		// last requested preset, so ViewportManager laid out every viewport and
		// divider for 1280x720 and the game drew into the top-left corner of its
		// own window with the rest cleared black.
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
			mattmath::Vector2I(width, height));

		std::ignore = this->renderer_->window_size_changed(width, height);
	}

	void Application::on_device_lost()
	{
		// Only the resources the device holds. Audio is not one, and tearing
		// down the sound banks here left every object holding a freed
		// SoundBank*. The sprite batches and the sampler states belong to the
		// renderer, which has already released them by the time this is called.
		this->render_resources_->impl()->release_all_textures();
		this->render_resources_->impl()->release_all_sprite_fonts();
	}

	void Application::on_device_restored()
	{
		// Reload the GPU-side assets into the existing RenderResources, so every
		// borrowed SpriteSheet* and SoundBank* stays valid.
		this->resource_loader_->set_device(
			this->renderer_->impl()->device_resources.GetD3DDevice());
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
			this->mouse_->on_move(mattmath::Vector2I(x, y));
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

	Keyboard* Application::keyboard() const
	{
		return this->keyboard_.get();
	}

	Mouse* Application::mouse() const
	{
		return this->mouse_.get();
	}

	Gamepads* Application::gamepads() const
	{
		return this->gamepads_.get();
	}
	HWND Application::window() const
	{
		return this->window_->handle();
	}

}

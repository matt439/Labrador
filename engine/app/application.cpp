#include "engine/app/application.h"
#include "engine/assets/asset_manifest_loader.h"
// The one place the shell has to name the backend: a window handle and a
// device belong to a platform, and this is the file that owns both.
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
		this->renderer_->create_device(this->window_, size.x, size.y,
			this->options_.max_threads);

		this->create_services();

		this->gamepad_reader_ = std::make_unique<GamepadReader>();
		this->gamepads_ = std::make_unique<Gamepads>(this->gamepad_reader_.get());

		this->timer_.SetFixedTimeStep(true);
		this->timer_.SetTargetElapsedSeconds(
			1.0 / static_cast<double>(this->options_.target_fps));
	}

	void Application::create_window(HINSTANCE instance, int show_command)
	{
		WNDCLASSEXW window_class = {};
		window_class.cbSize = sizeof(WNDCLASSEXW);
		window_class.style = CS_HREDRAW | CS_VREDRAW;
		window_class.lpfnWndProc = window_proc;
		window_class.hInstance = instance;
		window_class.hIcon = LoadIconW(instance, L"IDI_ICON");
		window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		window_class.lpszClassName = this->options_.window_class_name.c_str();
		window_class.hIconSm = LoadIconW(instance, L"IDI_ICON");

		if (RegisterClassExW(&window_class) == 0)
		{
			throw std::runtime_error("Could not register the window class.");
		}

		const mattmath::Vector2I client =
			this->resolution_manager_->resolution_ivec();

		const DWORD style = static_cast<DWORD>(
			this->options_.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW);
		const DWORD ex_style = static_cast<DWORD>(
			this->options_.fullscreen ? WS_EX_TOPMOST : 0);

		// THE REQUESTED SIZE IS CLIENT AREA. CreateWindowExW takes an OUTER
		// rect, so handing it the requested resolution straight spends the
		// caption and the borders out of the game's own pixels: a windowed
		// 1280x720 came out as roughly 1264x681 to draw into, at every preset,
		// and nothing said so. AdjustWindowRectEx is the only way to ask what
		// frame this style costs, and it answers zero for the WS_POPUP branch,
		// which is why one call covers both.
		const mattmath::Vector2I outer =
			outer_size_for_client(client, style, ex_style);

		// The Application pointer rides in as the create parameter and is stashed
		// in the window's user data by WM_CREATE, so window_proc can find it
		// without a global.
		this->window_ = CreateWindowExW(ex_style,
			this->options_.window_class_name.c_str(),
			this->options_.window_title.c_str(), style,
			CW_USEDEFAULT, CW_USEDEFAULT, outer.x, outer.y,
			nullptr, nullptr, instance, this);

		if (this->window_ == nullptr)
		{
			throw std::runtime_error("Could not create the window.");
		}

		// AND THE WM_SIZE THIS PRODUCES IS NOW WORTH SOMETHING. It arrives
		// before create_device, so the renderer half of on_window_size_changed
		// still does nothing - DeviceResources::WindowSizeChanged returns early
		// with no window set. The resolution-manager half does not, so by the
		// time initialize() reads resolution_ivec() for create_device, that is
		// the client size the window really got. It matters most for the
		// full-screen branch, where SW_SHOWMAXIMIZED decides the size and
		// nothing here knows it: the swap chain used to be created at the saved
		// preset and then stretched non-uniformly to the monitor
		// (DXGI_SCALING_STRETCH), which is the one form of this bug the shipped
		// sample hits.
		ShowWindow(this->window_,
			this->options_.fullscreen ? SW_SHOWMAXIMIZED : show_command);
	}

	mattmath::Vector2I Application::outer_size_for_client(
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

	mattmath::Vector2I Application::outer_size_for_client(
		const mattmath::Vector2I& client_size) const
	{
		return outer_size_for_client(client_size,
			static_cast<DWORD>(GetWindowLongPtrW(this->window_, GWL_STYLE)),
			static_cast<DWORD>(GetWindowLongPtrW(this->window_, GWL_EXSTYLE)));
	}

	// Services outlive the D3D device. They are created exactly once, here, and
	// never reassigned: every object a game builds snapshots raw pointers to them
	// at construction and is never told when they change. Recreating them on a
	// device restore turned the entire object graph into dangling pointers.
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
				this->tick();
			}
		}
		return static_cast<int>(message.wParam);
	}

	void Application::quit() const
	{
		if (this->window_ != nullptr)
		{
			DestroyWindow(this->window_);
		}
	}

	void Application::set_resolution(ScreenResolution resolution)
	{
		this->options_.resolution = resolution;
		this->resolution_manager_->set_resolution(resolution);

		// Adjusted for whatever frame the window is currently wearing, so the
		// game gets the client area it asked for rather than that minus a
		// caption. The WM_SIZE this produces re-points the resolution manager at
		// what the window actually became, which is not always what was asked
		// for - a size past the monitor's comes back clamped.
		const mattmath::Vector2I outer = this->outer_size_for_client(
			this->resolution_manager_->resolution_ivec());
		SetWindowPos(this->window_, HWND_TOP, 0, 0, outer.x, outer.y,
			SWP_NOMOVE | SWP_NOZORDER);
	}

	void Application::set_fullscreen(bool fullscreen)
	{
		this->options_.fullscreen = fullscreen;

		if (fullscreen)
		{
			SetWindowLongPtr(this->window_, GWL_STYLE, WS_POPUP);
			SetWindowLongPtr(this->window_, GWL_EXSTYLE, WS_EX_TOPMOST);
			SetWindowPos(this->window_, HWND_TOP, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			ShowWindow(this->window_, SW_SHOWMAXIMIZED);
		}
		else
		{
			SetWindowLongPtr(this->window_, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			SetWindowLongPtr(this->window_, GWL_EXSTYLE, 0);

			// BACK TO THE SIZE THE GAME ASKED FOR, WHICH IS NO LONGER THE SIZE
			// THE MANAGER HOLDS. resolution_ivec() used to be a synonym for the
			// requested preset and is now the live window, so while full screen
			// it is the monitor - reading it here would keep the window at
			// monitor size with a caption on it. options_.resolution is the last
			// thing a game actually requested, so the manager is re-pointed at
			// it, and the WM_SIZE below then corrects it to whatever client area
			// that yields. The style is set before this runs, so the frame
			// arithmetic below reads the ordinary window's frame and not
			// WS_POPUP's nothing.
			this->resolution_manager_->set_resolution(this->options_.resolution);
			const mattmath::Vector2I outer = this->outer_size_for_client(
				this->resolution_manager_->resolution_ivec());

			ShowWindow(this->window_, SW_SHOWNORMAL);
			SetWindowPos(this->window_, HWND_TOP, 0, 0, outer.x, outer.y,
				SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
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
		this->gamepads_->poll();

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

	void Application::on_activated() const
	{
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->resume();
		}
	}

	void Application::on_deactivated() const
	{
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->suspend();
		}
	}

	void Application::on_suspending() const
	{
		if (this->gamepad_reader_)
		{
			this->gamepad_reader_->suspend();
		}
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
		if (this->audio_engine_)
		{
			this->audio_engine_->Resume();
		}
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
	Gamepads* Application::gamepads() const
	{
		return this->gamepads_.get();
	}
	HWND Application::window() const
	{
		return this->window_;
	}

	// Every message either forwards to the Application or is Windows housekeeping.
	// There is nothing game-specific here, which is the reason it is in the engine
	// and not copied into every project's main.cpp.
	LRESULT CALLBACK Application::window_proc(HWND window, UINT message,
		WPARAM w_param, LPARAM l_param)
	{
		static bool in_sizemove = false;
		static bool in_suspend = false;
		static bool minimized = false;

		auto* app = reinterpret_cast<Application*>(
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
			if (in_sizemove && app)
			{
				app->tick();
			}
			else
			{
				PAINTSTRUCT paint;
				std::ignore = BeginPaint(window, &paint);
				EndPaint(window, &paint);
			}
			break;

		case WM_DISPLAYCHANGE:
			if (app)
			{
				app->on_display_change();
			}
			break;

		case WM_MOVE:
			if (app)
			{
				app->on_window_moved();
			}
			break;

		case WM_SIZE:
			if (w_param == SIZE_MINIMIZED)
			{
				if (!minimized)
				{
					minimized = true;
					if (!in_suspend && app)
					{
						app->on_suspending();
					}
					in_suspend = true;
				}
			}
			else if (minimized)
			{
				minimized = false;
				if (in_suspend && app)
				{
					app->on_resuming();
				}
				in_suspend = false;
			}
			else if (!in_sizemove && app)
			{
				app->on_window_size_changed(LOWORD(l_param), HIWORD(l_param));
			}
			break;

		case WM_ENTERSIZEMOVE:
			in_sizemove = true;
			break;

		case WM_EXITSIZEMOVE:
			in_sizemove = false;
			if (app)
			{
				RECT client;
				GetClientRect(window, &client);
				app->on_window_size_changed(client.right - client.left,
					client.bottom - client.top);
			}
			break;

		case WM_GETMINMAXINFO:
			if (l_param && app)
			{
				auto info = reinterpret_cast<MINMAXINFO*>(l_param);
				info->ptMinTrackSize.x = app->options_.min_window_width;
				info->ptMinTrackSize.y = app->options_.min_window_height;
			}
			break;

		case WM_ACTIVATEAPP:
			if (app)
			{
				if (w_param)
				{
					app->on_activated();
				}
				else
				{
					app->on_deactivated();
				}
			}
			break;

		case WM_POWERBROADCAST:
			switch (w_param)
			{
			case PBT_APMQUERYSUSPEND:
				if (!in_suspend && app)
				{
					app->on_suspending();
				}
				in_suspend = true;
				return TRUE;

			case PBT_APMRESUMESUSPEND:
				if (!minimized)
				{
					if (in_suspend && app)
					{
						app->on_resuming();
					}
					in_suspend = false;
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

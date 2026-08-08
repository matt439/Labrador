#include "engine/app/application.h"
#include "engine/assets/asset_manifest_loader.h"
// The one place the shell has to name the backend: a window handle and a
// device belong to a platform, and this is the file that owns both.
#include "engine/render/d3d11/backend.h"
#include <DirectXMath.h>
#include <objbase.h>
#include <stdexcept>
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

		this->gamepad_ = std::make_unique<GamePad>();

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

		const mattmath::Vector2I size =
			this->resolution_manager_->resolution_ivec();

		// The Application pointer rides in as the create parameter and is stashed
		// in the window's user data by WM_CREATE, so window_proc can find it
		// without a global.
		if (this->options_.fullscreen)
		{
			this->window_ = CreateWindowExW(WS_EX_TOPMOST,
				this->options_.window_class_name.c_str(),
				this->options_.window_title.c_str(), WS_POPUP,
				CW_USEDEFAULT, CW_USEDEFAULT, size.x, size.y,
				nullptr, nullptr, instance, this);
		}
		else
		{
			this->window_ = CreateWindowExW(0,
				this->options_.window_class_name.c_str(),
				this->options_.window_title.c_str(), WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, size.x, size.y,
				nullptr, nullptr, instance, this);
		}

		if (this->window_ == nullptr)
		{
			throw std::runtime_error("Could not create the window.");
		}

		ShowWindow(this->window_,
			this->options_.fullscreen ? SW_SHOWMAXIMIZED : show_command);
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

		const mattmath::Vector2I size =
			this->resolution_manager_->resolution_ivec();
		SetWindowPos(this->window_, HWND_TOP, 0, 0, size.x, size.y,
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

			const mattmath::Vector2I size =
				this->resolution_manager_->resolution_ivec();
			ShowWindow(this->window_, SW_SHOWNORMAL);
			SetWindowPos(this->window_, HWND_TOP, 0, 0, size.x, size.y,
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
		if (this->gamepad_)
		{
			this->gamepad_->Resume();
		}
	}

	void Application::on_deactivated() const
	{
		if (this->gamepad_)
		{
			this->gamepad_->Suspend();
		}
	}

	void Application::on_suspending() const
	{
		if (this->gamepad_)
		{
			this->gamepad_->Suspend();
		}
		if (this->audio_engine_)
		{
			this->audio_engine_->Suspend();
		}
	}

	void Application::on_resuming()
	{
		this->timer_.ResetElapsedTime();
		if (this->gamepad_)
		{
			this->gamepad_->Resume();
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
	GamePad* Application::gamepad() const
	{
		return this->gamepad_.get();
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

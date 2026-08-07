#include "engine/app/application.h"
#include "engine/assets/asset_manifest_loader.h"
#include <DirectXMath.h>
#include <objbase.h>
#include <stdexcept>
#include <tuple>

using namespace DirectX;

Application::Application(ApplicationOptions options) :
	_options(std::move(options))
{
	// Renders only 2D, so no depth buffer.
	this->_device_resources = std::make_unique<DX::DeviceResources>(
		DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
	this->_device_resources->RegisterDeviceNotify(this);
}

Application::~Application()
{
	if (this->_audio_engine)
	{
		this->_audio_engine->Suspend();
	}
	if (this->_com_initialized)
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
	this->_com_initialized = true;

	this->_resolution_manager = std::make_unique<ResolutionManager>();
	this->_resolution_manager->set_resolution(this->_options.resolution);

	this->create_window(instance, show_command);

	AUDIO_ENGINE_FLAGS audio_flags = AudioEngine_Default;
#ifdef _DEBUG
	audio_flags |= AudioEngine_Debug;
#endif
	this->_audio_engine = std::make_unique<AudioEngine>(audio_flags);

	const MattMath::Vector2I size =
		this->_resolution_manager->get_resolution_ivec();
	this->_device_resources->SetWindow(this->_window, size.x, size.y);
	this->_device_resources->CreateDeviceResources();
	this->_device_resources->create_deferred_contexts(
		this->_options.max_threads);

	this->create_services();
	this->create_device_dependent_resources();

	this->_device_resources->CreateWindowSizeDependentResources();

	this->_gamepad = std::make_unique<GamePad>();

	this->_timer.SetFixedTimeStep(true);
	this->_timer.SetTargetElapsedSeconds(
		1.0 / static_cast<double>(this->_options.target_fps));
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
	window_class.lpszClassName = this->_options.window_class_name.c_str();
	window_class.hIconSm = LoadIconW(instance, L"IDI_ICON");

	if (RegisterClassExW(&window_class) == 0)
	{
		throw std::runtime_error("Could not register the window class.");
	}

	const MattMath::Vector2I size =
		this->_resolution_manager->get_resolution_ivec();

	// The Application pointer rides in as the create parameter and is stashed
	// in the window's user data by WM_CREATE, so window_proc can find it
	// without a global.
	if (this->_options.fullscreen)
	{
		this->_window = CreateWindowExW(WS_EX_TOPMOST,
			this->_options.window_class_name.c_str(),
			this->_options.window_title.c_str(), WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, size.x, size.y,
			nullptr, nullptr, instance, this);
	}
	else
	{
		this->_window = CreateWindowExW(0,
			this->_options.window_class_name.c_str(),
			this->_options.window_title.c_str(), WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, size.x, size.y,
			nullptr, nullptr, instance, this);
	}

	if (this->_window == nullptr)
	{
		throw std::runtime_error("Could not create the window.");
	}

	ShowWindow(this->_window,
		this->_options.fullscreen ? SW_SHOWMAXIMIZED : show_command);
}

// Services outlive the D3D device. They are created exactly once, here, and
// never reassigned: every object a game builds snapshots raw pointers to them
// at construction and is never told when they change. Recreating them on a
// device restore turned the entire object graph into dangling pointers.
void Application::create_services()
{
	this->_thread_pool = std::make_unique<ThreadPool>(
		this->_options.min_threads, this->_options.max_threads);

	this->_render_resources = std::make_unique<RenderResources>();
	this->_audio_resources = std::make_unique<AudioResources>();

	this->_resource_loader = std::make_unique<ResourceLoader>(
		this->_render_resources.get(), this->_audio_resources.get(),
		this->_device_resources->GetD3DDevice(), this->_audio_engine.get());

	this->_viewport_manager = std::make_unique<ViewportManager>(
		this->_resolution_manager.get(), this->_device_resources.get());

	this->_partitioner = std::make_unique<Partitioner>();

	this->_dt = std::make_unique<float>(0.0f);
}

void Application::create_device_dependent_resources()
{
	ID3D11Device1* device = this->_device_resources->GetD3DDevice();

	this->_sprite_batches.resize(
		static_cast<size_t>(this->_options.max_threads));
	this->_sprite_batch_ptrs.resize(
		static_cast<size_t>(this->_options.max_threads));
	for (int i = 0; i < this->_options.max_threads; i++)
	{
		this->_sprite_batches[static_cast<size_t>(i)] =
			std::make_unique<SpriteBatch>(
				this->_device_resources->get_deferred_context(i));
		this->_sprite_batch_ptrs[static_cast<size_t>(i)] =
			this->_sprite_batches[static_cast<size_t>(i)].get();
	}

	this->_common_states = std::make_unique<CommonStates>(device);

	// Reload the GPU-side assets into the existing RenderResources, so every
	// borrowed SpriteSheet* and SoundBank* stays valid.
	this->_resource_loader->set_device(device);
	if (this->_content_loaded)
	{
		this->_resource_loader->reload_device_resources();
	}
}

void Application::load_manifest(const std::string& manifest_path)
{
	this->_resource_loader->load_manifest(
		asset_manifest_loader::load(manifest_path.c_str()));
	this->_content_loaded = true;
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
	if (this->_window != nullptr)
	{
		DestroyWindow(this->_window);
	}
}

void Application::set_resolution(screen_resolution resolution)
{
	this->_options.resolution = resolution;
	this->_resolution_manager->set_resolution(resolution);

	const MattMath::Vector2I size =
		this->_resolution_manager->get_resolution_ivec();
	SetWindowPos(this->_window, HWND_TOP, 0, 0, size.x, size.y,
		SWP_NOMOVE | SWP_NOZORDER);
}

void Application::set_fullscreen(bool fullscreen)
{
	this->_options.fullscreen = fullscreen;

	if (fullscreen)
	{
		SetWindowLongPtr(this->_window, GWL_STYLE, WS_POPUP);
		SetWindowLongPtr(this->_window, GWL_EXSTYLE, WS_EX_TOPMOST);
		SetWindowPos(this->_window, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowWindow(this->_window, SW_SHOWMAXIMIZED);
	}
	else
	{
		SetWindowLongPtr(this->_window, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowLongPtr(this->_window, GWL_EXSTYLE, 0);

		const MattMath::Vector2I size =
			this->_resolution_manager->get_resolution_ivec();
		ShowWindow(this->_window, SW_SHOWNORMAL);
		SetWindowPos(this->_window, HWND_TOP, 0, 0, size.x, size.y,
			SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}
}

void Application::tick()
{
	this->_timer.Tick([&]() { this->update(); });
	this->render();
}

void Application::update()
{
	*this->_dt = static_cast<float>(this->_timer.GetElapsedSeconds());
	StateContext::update();
	std::ignore = this->_audio_engine->Update();
}

void Application::render()
{
	// Nothing to draw before the first update has run.
	if (this->_timer.GetFrameCount() == 0)
	{
		return;
	}

	this->clear();

	this->_device_resources->PIXBeginEvent(L"Render");
	this->draw();
	this->_device_resources->PIXEndEvent();

	this->_device_resources->Present();
}

void Application::clear() const
{
	this->_device_resources->PIXBeginEvent(L"Clear");

	ID3D11DeviceContext1* context = this->_device_resources->GetD3DDeviceContext();
	auto deferred_contexts = this->_device_resources->get_deferred_contexts();
	ID3D11RenderTargetView* render_target =
		this->_device_resources->GetRenderTargetView();

	context->ClearRenderTargetView(render_target, Colors::Black);
	context->OMSetRenderTargets(1, &render_target, nullptr);

	auto const viewport = this->_device_resources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// Every worker draws into its own deferred context, so each needs the
	// same target and viewport bound before the frame fans out.
	for (auto& deferred_context : *deferred_contexts)
	{
		deferred_context->OMSetRenderTargets(1, &render_target, nullptr);
		deferred_context->RSSetViewports(1, &viewport);
	}

	this->_device_resources->PIXEndEvent();
}

void Application::on_activated() const
{
	if (this->_gamepad)
	{
		this->_gamepad->Resume();
	}
}

void Application::on_deactivated() const
{
	if (this->_gamepad)
	{
		this->_gamepad->Suspend();
	}
}

void Application::on_suspending() const
{
	if (this->_gamepad)
	{
		this->_gamepad->Suspend();
	}
	if (this->_audio_engine)
	{
		this->_audio_engine->Suspend();
	}
}

void Application::on_resuming()
{
	this->_timer.ResetElapsedTime();
	if (this->_gamepad)
	{
		this->_gamepad->Resume();
	}
	if (this->_audio_engine)
	{
		this->_audio_engine->Resume();
	}
}

void Application::on_window_moved() const
{
	auto const bounds = this->_device_resources->GetOutputSize();
	this->_device_resources->WindowSizeChanged(bounds.right, bounds.bottom);
}

void Application::on_display_change() const
{
	this->_device_resources->UpdateColorSpace();
}

void Application::on_window_size_changed(int width, int height)
{
	std::ignore = this->_device_resources->WindowSizeChanged(width, height);
}

void Application::OnDeviceLost()
{
	// Only D3D objects. Audio is not a device resource, and tearing down the
	// sound banks here left every object holding a freed SoundBank*.
	for (auto& sprite_batch : this->_sprite_batches)
	{
		sprite_batch.reset();
	}
	this->_sprite_batch_ptrs.assign(this->_sprite_batch_ptrs.size(), nullptr);

	this->_common_states.reset();

	this->_render_resources->reset_all_textures();
	this->_render_resources->reset_all_sprite_fonts();
}

void Application::OnDeviceRestored()
{
	this->create_device_dependent_resources();
}

DX::DeviceResources* Application::device_resources() const
{
	return this->_device_resources.get();
}
RenderResources* Application::render_resources() const
{
	return this->_render_resources.get();
}
AudioResources* Application::audio_resources() const
{
	return this->_audio_resources.get();
}
ResourceLoader* Application::resource_loader() const
{
	return this->_resource_loader.get();
}
ResolutionManager* Application::resolution_manager() const
{
	return this->_resolution_manager.get();
}
ViewportManager* Application::viewport_manager() const
{
	return this->_viewport_manager.get();
}
ThreadPool* Application::thread_pool() const
{
	return this->_thread_pool.get();
}
const Partitioner* Application::partitioner() const
{
	return this->_partitioner.get();
}
GamePad* Application::gamepad() const
{
	return this->_gamepad.get();
}
HWND Application::window() const
{
	return this->_window;
}
CommonStates* Application::common_states() const
{
	return this->_common_states.get();
}
std::vector<SpriteBatch*>* Application::sprite_batches() const
{
	return const_cast<std::vector<SpriteBatch*>*>(&this->_sprite_batch_ptrs);
}
const float* Application::dt() const
{
	return this->_dt.get();
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
			info->ptMinTrackSize.x = app->_options.min_window_width;
			info->ptMinTrackSize.y = app->_options.min_window_height;
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

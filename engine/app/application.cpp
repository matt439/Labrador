#include "engine/app/application.h"
#include "engine/assets/asset_manifest_loader.h"
#include <DirectXMath.h>
#include <objbase.h>
#include <stdexcept>
#include <tuple>

using namespace DirectX;

Application::Application(ApplicationOptions options) :
	options_(std::move(options))
{
	// Renders only 2D, so no depth buffer.
	this->device_resources_ = std::make_unique<DX::DeviceResources>(
		DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
	this->device_resources_->RegisterDeviceNotify(this);
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
	this->device_resources_->SetWindow(this->window_, size.x, size.y);
	this->device_resources_->CreateDeviceResources();
	this->device_resources_->create_deferred_contexts(
		this->options_.max_threads);

	this->create_services();
	this->create_device_dependent_resources();

	this->device_resources_->CreateWindowSizeDependentResources();

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

	this->resource_loader_ = std::make_unique<ResourceLoader>(
		this->render_resources_.get(), this->audio_resources_.get(),
		this->device_resources_->GetD3DDevice(), this->audio_engine_.get());

	this->viewport_manager_ = std::make_unique<ViewportManager>(
		this->resolution_manager_.get(), this->device_resources_.get());

	this->partitioner_ = std::make_unique<Partitioner>();

	this->dt_ = std::make_unique<float>(0.0f);
}

void Application::create_device_dependent_resources()
{
	ID3D11Device1* device = this->device_resources_->GetD3DDevice();

	this->sprite_batches_.resize(
		static_cast<size_t>(this->options_.max_threads));
	this->sprite_batch_ptrs_.resize(
		static_cast<size_t>(this->options_.max_threads));
	for (int i = 0; i < this->options_.max_threads; i++)
	{
		this->sprite_batches_[static_cast<size_t>(i)] =
			std::make_unique<SpriteBatch>(
				this->device_resources_->deferred_context(i));
		this->sprite_batch_ptrs_[static_cast<size_t>(i)] =
			this->sprite_batches_[static_cast<size_t>(i)].get();
	}

	this->common_states_ = std::make_unique<CommonStates>(device);

	// Reload the GPU-side assets into the existing RenderResources, so every
	// borrowed SpriteSheet* and SoundBank* stays valid.
	this->resource_loader_->set_device(device);
	if (this->content_loaded_)
	{
		this->resource_loader_->reload_device_resources();
	}
}

void Application::load_manifest(const std::string& manifest_path)
{
	this->resource_loader_->load_manifest(
		asset_manifest_loader::load(manifest_path.c_str()));
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

void Application::tick()
{
	this->timer_.Tick([&]() { this->update(); });
	this->render();
}

void Application::update()
{
	*this->dt_ = static_cast<float>(this->timer_.GetElapsedSeconds());
	StateContext::update();
	std::ignore = this->audio_engine_->Update();
}

void Application::render()
{
	// Nothing to draw before the first update has run.
	if (this->timer_.GetFrameCount() == 0)
	{
		return;
	}

	this->clear();

	this->device_resources_->PIXBeginEvent(L"Render");
	this->draw();
	this->device_resources_->PIXEndEvent();

	this->device_resources_->Present();
}

void Application::clear() const
{
	this->device_resources_->PIXBeginEvent(L"Clear");

	ID3D11DeviceContext1* context = this->device_resources_->GetD3DDeviceContext();
	auto deferred_contexts = this->device_resources_->deferred_contexts();
	ID3D11RenderTargetView* render_target =
		this->device_resources_->GetRenderTargetView();

	context->ClearRenderTargetView(render_target, Colors::Black);
	context->OMSetRenderTargets(1, &render_target, nullptr);

	auto const viewport = this->device_resources_->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// Every worker draws into its own deferred context, so each needs the
	// same target and viewport bound before the frame fans out.
	for (auto& deferred_context : *deferred_contexts)
	{
		deferred_context->OMSetRenderTargets(1, &render_target, nullptr);
		deferred_context->RSSetViewports(1, &viewport);
	}

	this->device_resources_->PIXEndEvent();
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
	auto const bounds = this->device_resources_->GetOutputSize();
	this->device_resources_->WindowSizeChanged(bounds.right, bounds.bottom);
}

void Application::on_display_change() const
{
	this->device_resources_->UpdateColorSpace();
}

void Application::on_window_size_changed(int width, int height)
{
	std::ignore = this->device_resources_->WindowSizeChanged(width, height);
}

void Application::OnDeviceLost()
{
	// Only D3D objects. Audio is not a device resource, and tearing down the
	// sound banks here left every object holding a freed SoundBank*.
	for (auto& sprite_batch : this->sprite_batches_)
	{
		sprite_batch.reset();
	}
	this->sprite_batch_ptrs_.assign(this->sprite_batch_ptrs_.size(), nullptr);

	this->common_states_.reset();

	this->render_resources_->reset_all_textures();
	this->render_resources_->reset_all_sprite_fonts();
}

void Application::OnDeviceRestored()
{
	this->create_device_dependent_resources();
}

DX::DeviceResources* Application::device_resources() const
{
	return this->device_resources_.get();
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
CommonStates* Application::common_states() const
{
	return this->common_states_.get();
}
std::vector<SpriteBatch*>* Application::sprite_batches() const
{
	return const_cast<std::vector<SpriteBatch*>*>(&this->sprite_batch_ptrs_);
}
const float* Application::dt() const
{
	return this->dt_.get();
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

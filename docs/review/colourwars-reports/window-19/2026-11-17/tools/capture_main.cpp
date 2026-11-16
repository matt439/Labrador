#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/renderer.h"
#include "samples/minimal/states/hello_state.h"
#include "tools/linesweeper_capture/png_file.h"

#include <Windows.h>
#include <dwmapi.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using namespace labrador;

namespace
{
	struct Observation
	{
		std::ofstream log;
		std::filesystem::path directory;
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		bool first_draw = false;
		bool captured = false;
	};

	void record(Observation& observation, Application& app, const char* event)
	{
		const HWND window = static_cast<HWND>(app.window());
		RECT outer = {};
		RECT client = {};
		RECT frame = {};
		POINT origin = {};
		if (window)
		{
			std::ignore = GetWindowRect(window, &outer);
			std::ignore = GetClientRect(window, &client);
			std::ignore = ClientToScreen(window, &origin);
			std::ignore = DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
				&frame, sizeof(frame));
		}
		const Vector2F buffer = app.renderer()->back_buffer_size();
		const double elapsed = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - observation.start).count();
		observation.log << "{\"event\":" << std::quoted(event)
			<< ",\"utc_epoch_ms\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count()
			<< ",\"elapsed_ms\":" << elapsed
			<< ",\"frame\":" << app.frame_timings().frame
			<< ",\"visible\":" << (window && IsWindowVisible(window) ? "true" : "false")
			<< ",\"foreground\":" << (window && GetForegroundWindow() == window ? "true" : "false")
			<< ",\"fullscreen\":" << (app.fullscreen() ? "true" : "false")
			<< ",\"client_origin\":[" << origin.x << ',' << origin.y << ']'
			<< ",\"client_size\":[" << client.right << ',' << client.bottom << ']'
			<< ",\"window_rect\":[" << outer.left << ',' << outer.top << ','
			<< outer.right << ',' << outer.bottom << ']'
			<< ",\"frame_rect\":[" << frame.left << ',' << frame.top << ','
			<< frame.right << ',' << frame.bottom << ']'
			<< ",\"window_dpi\":" << (window ? GetDpiForWindow(window) : 0)
			<< ",\"buffer_size\":[" << buffer.x << ',' << buffer.y << "]}\n";
		observation.log.flush();
	}

	class CaptureState : public State
	{
	public:
		CaptureState(Application* app, Observation* observation) :
			app_(app), observation_(observation), game_(std::make_unique<HelloState>(app))
		{
		}

		void init() override
		{
			this->game_->set_context(this->context());
			this->game_->init();
			record(*this->observation_, *this->app_, "state_init");
		}

		void update(float dt) override
		{
			this->game_->update(dt);
			if (!this->observation_->captured && this->app_->frame_timings().frame > 0)
			{
				this->observation_->captured = true;
				record(*this->observation_, *this->app_, "observed_completed_frame");
				Renderer& renderer = *this->app_->renderer();
				renderer.begin_frame();
				this->app_->draw(renderer);
				renderer.submit();
				std::vector<unsigned char> pixels;
				renderer.read_back_buffer(pixels);
				const Vector2F size = renderer.back_buffer_size();
				capture::write_png(this->observation_->directory / "client-redraw.png",
					static_cast<int>(size.x), static_cast<int>(size.y), pixels);
				record(*this->observation_, *this->app_, "client_redraw_exported");
			}
			if (std::chrono::steady_clock::now() - this->observation_->start >
				std::chrono::seconds(90))
			{
				this->app_->quit();
			}
		}

		void draw(Renderer& renderer) const override
		{
			const bool first = !this->observation_->first_draw;
			if (first)
			{
				this->observation_->first_draw = true;
				record(*this->observation_, *this->app_, "first_draw_begin");
			}
			this->game_->draw(renderer);
			if (first)
			{
				record(*this->observation_, *this->app_, "first_draw_end");
			}
		}

		void on_activated() override
		{
			record(*this->observation_, *this->app_, "state_activated");
			this->game_->on_activated();
		}
		void on_deactivated() override
		{
			record(*this->observation_, *this->app_, "state_deactivated");
			this->game_->on_deactivated();
		}

	private:
		Application* app_;
		Observation* observation_;
		std::unique_ptr<HelloState> game_;
	};
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	wchar_t executable[32768] = {};
	const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
	const std::filesystem::path path(std::wstring(executable, length));
	const bool fullscreen = CAPTURE_FULLSCREEN != 0;
	Observation observation;
	observation.directory = path.parent_path() / (fullscreen ? "fullscreen" : "windowed");
	std::filesystem::create_directories(observation.directory);
	observation.log.open(observation.directory / "events.jsonl");
	try
	{
		ApplicationOptions options;
		options.window_class_name = L"Oca87CaptureWindow";
		options.window_title = fullscreen ? L"Labrador OCA-87 capture - fullscreen" :
			L"Labrador OCA-87 capture - windowed";
		options.fullscreen = fullscreen;
		options.max_threads = 1;
		options.view_capacity = 1;
		Application app(options);
		app.initialize();
		record(observation, app, "initialized");
		const RenderDeviceInfo& device = app.renderer()->device_info();
		observation.log << "{\"event\":\"device\",\"backend\":" << std::quoted(device.backend)
			<< ",\"module_path\":" << std::quoted(path.string())
			<< ",\"api\":" << std::quoted(device.api)
			<< ",\"name\":" << std::quoted(device.device_name)
			<< ",\"kind\":" << static_cast<int>(device.kind)
			<< ",\"vendor_id\":" << device.vendor_id
			<< ",\"device_id\":" << device.device_id << "}\n";
		app.load_manifest("manifest.json");
		record(observation, app, "manifest_loaded");
		const int result = app.run(std::make_unique<CaptureState>(&app, &observation));
		record(observation, app, "run_returned");
		return result;
	}
	catch (const std::exception& error)
	{
		observation.log << "{\"event\":\"error\",\"message\":" << std::quoted(error.what()) << "}\n";
		return 1;
	}
}

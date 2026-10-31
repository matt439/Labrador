#include "samples/collision/collision_state.h"

#include "engine/app/application.h"
#ifdef COLLISION_SAMPLE_CAPTURE
#include "tools/linesweeper_capture/png_file.h"
#endif

#include <Windows.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace collisiondemo;
using namespace labrador;

namespace
{
	std::wstring_view trim_arguments(std::wstring_view arguments)
	{
		const std::size_t first = arguments.find_first_not_of(L" \t\r\n");
		if (first == std::wstring_view::npos)
		{
			return {};
		}
		const std::size_t last = arguments.find_last_not_of(L" \t\r\n");
		return arguments.substr(first, last - first + 1);
	}
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE,
	_In_ LPWSTR command_line, _In_ int)
{
	const std::wstring_view arguments = trim_arguments(command_line);
	const bool smoke_test = arguments.starts_with(L"--smoke-test");
	try
	{
		std::filesystem::path capture_path;
		constexpr std::wstring_view capture_prefix = L"--smoke-test --capture ";
		if (arguments.starts_with(capture_prefix))
		{
			std::wstring_view path = arguments.substr(capture_prefix.size());
			if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
			{
				path = path.substr(1, path.size() - 2);
			}
			if (path.empty()) throw std::invalid_argument("Capture path must not be empty");
			capture_path = path;
#ifndef COLLISION_SAMPLE_CAPTURE
			throw std::invalid_argument("The null renderer cannot capture pixels");
#endif
		}
		else if (!arguments.empty() && arguments != L"--smoke-test")
		{
			throw std::invalid_argument("usage: CollisionSample [--smoke-test [--capture path.png]]");
		}
		ApplicationOptions options;
		options.window_class_name = L"CollisionSampleWindow";
		options.window_title = L"Labrador - collision sample";
		options.view_capacity = 1;
		options.max_threads = 1;
		options.visible = !smoke_test;
		Application app(std::move(options));
		app.initialize();
		app.load_manifest("./manifest.json");
		if (smoke_test)
		{
			CollisionState state(&app, true);
			state.init();
			for (int tick = 0; tick < 315; ++tick)
			{
				state.update(1.0f / 60.0f);
				app.renderer()->begin_frame();
				state.draw(*app.renderer());
				app.renderer()->submit();
#ifdef COLLISION_SAMPLE_CAPTURE
				if (tick == 314 && !capture_path.empty())
				{
					std::vector<unsigned char> pixels;
					app.renderer()->read_back_buffer(pixels);
					const Vector2F size = app.renderer()->back_buffer_size();
					capture::write_png(capture_path, static_cast<int>(size.x),
						static_cast<int>(size.y), pixels);
				}
#endif
				app.renderer()->end_frame();
			}
			state.verify_smoke_test();
			return 0;
		}
		return app.run(std::make_unique<CollisionState>(&app, smoke_test));
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "CollisionSample: %s\n", error.what());
		if (arguments.empty())
		{
			MessageBoxA(nullptr, error.what(), "CollisionSample failure", MB_OK | MB_ICONERROR);
		}
		return 1;
	}
}

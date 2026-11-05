#include "engine/app/application.h"
#include "samples/local_multiplayer/join_state.h"
#include "samples/local_multiplayer/play_state.h"
#ifdef LOCAL_MULTIPLAYER_CAPTURE
#include "tools/linesweeper_capture/png_file.h"
#endif

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace labrador;

namespace
{
	// The smoke-test frame --capture writes: the fifteenth, which the README's
	// Verification section names.
	[[maybe_unused]] constexpr int capture_tick = 14;

	// The arguments after the program's name, split and unquoted as Windows
	// does it, so a capture path with a space in it arrives whole.
	std::vector<std::wstring> command_arguments()
	{
		int count = 0;
		const auto release = [](wchar_t** arguments) { LocalFree(arguments); };
		const std::unique_ptr<wchar_t*, decltype(release)> arguments(
			CommandLineToArgvW(GetCommandLineW(), &count), release);
		if (arguments == nullptr)
		{
			throw std::runtime_error("Could not read command-line arguments");
		}
		std::vector<std::wstring> result;
		for (int index = 1; index < count; ++index)
		{
			result.emplace_back(arguments.get()[index]);
		}
		return result;
	}
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE,
	_In_ LPWSTR, _In_ int)
{
	// A smoke test has nobody to close a message box, so its failure goes to
	// the standard error stream alone; any other failure, a mistyped argument
	// included, is shown as well.
	bool smoke_test = false;
	try
	{
		const std::vector<std::wstring> arguments = command_arguments();
		smoke_test = !arguments.empty() && arguments[0] == L"--smoke-test";
		std::filesystem::path capture_path;
		if (smoke_test && arguments.size() == 3 && arguments[1] == L"--capture")
		{
			if (arguments[2].empty()) throw std::invalid_argument("Capture path must not be empty");
			capture_path = arguments[2];
#ifndef LOCAL_MULTIPLAYER_CAPTURE
			throw std::invalid_argument("The null renderer cannot capture pixels");
#endif
		}
		else if (!arguments.empty() && !(smoke_test && arguments.size() == 1))
		{
			throw std::invalid_argument("usage: LocalMultiplayerSample [--smoke-test [--capture path.png]]");
		}
		ApplicationOptions options;
		options.window_class_name = L"LocalMultiplayerSampleWindow";
		options.window_title = L"Labrador - local multiplayer";
		options.resolution = ScreenResolution::s_1280_720;
		options.view_capacity = 2;
		options.max_threads = std::min(2, default_thread_count());
		options.visible = !smoke_test;
		Application app(std::move(options));
		app.initialize();
		app.load_manifest("./manifest.json");

		if (smoke_test)
		{
			multiplayer::PlayState state(&app, multiplayer::SmokeTest{});
			state.init();
			for (int tick = 0; tick < multiplayer::smoke_ticks; ++tick)
			{
				state.update(multiplayer::smoke_step_time);
				app.renderer()->begin_frame();
				state.draw(*app.renderer());
				app.renderer()->submit();
#ifdef LOCAL_MULTIPLAYER_CAPTURE
				if (tick == capture_tick && !capture_path.empty())
				{
					std::vector<unsigned char> pixels;
					app.renderer()->read_back_buffer(pixels);
					const labrador::Vector2F size = app.renderer()->back_buffer_size();
					capture::write_png(capture_path, static_cast<int>(size.x),
						static_cast<int>(size.y), pixels);
				}
#endif
				app.renderer()->end_frame();
			}
			state.verify_smoke_test();
			return 0;
		}
		return app.run(std::make_unique<multiplayer::JoinState>(&app));
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "LocalMultiplayerSample: %s\n", error.what());
		if (!smoke_test)
		{
			MessageBoxA(nullptr, error.what(), "LocalMultiplayerSample failure", MB_OK | MB_ICONERROR);
		}
		return 1;
	}
}

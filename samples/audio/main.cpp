#include "engine/app/application.h"
#include "samples/audio/audio_state.h"
#include "samples/audio/smoke_test.h"

#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
	// The arguments after the program's name, split and unquoted as Windows
	// does it.
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
	bool com_initialized = false;
	try
	{
		const std::vector<std::wstring> arguments = command_arguments();
		smoke_test = !arguments.empty() && arguments[0] == L"--smoke-test";
		if (!arguments.empty() && !(smoke_test && arguments.size() == 1))
		{
			throw std::invalid_argument("usage: AudioSample [--smoke-test]");
		}
		if (smoke_test)
		{
			if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
			{
				throw std::runtime_error("Could not initialize COM");
			}
			com_initialized = true;
			audio::smoke_test();
			CoUninitialize();
			return 0;
		}
		labrador::ApplicationOptions options;
		options.window_class_name = L"AudioSampleWindowClass";
		options.window_title = L"Labrador - audio sample";
		options.view_capacity = 1;
		options.max_threads = 1;
		labrador::Application app(std::move(options));
		app.initialize();
		app.load_manifest("./manifest.json");
		return app.run(std::make_unique<audio::AudioState>(&app));
	}
	catch (const std::exception& error)
	{
		if (com_initialized) { CoUninitialize(); }
		std::fprintf(stderr, "AudioSample: %s\n", error.what());
		if (!smoke_test)
		{
			MessageBoxA(nullptr, error.what(), "AudioSample failure", MB_OK | MB_ICONERROR);
		}
		return 1;
	}
}

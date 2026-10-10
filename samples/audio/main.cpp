#include "engine/app/application.h"
#include "samples/audio/audio_state.h"
#include "samples/audio/smoke_test.h"

#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <cwchar>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE,
	_In_ LPWSTR command_line, _In_ int)
{
	const bool interactive = command_line[0] == L'\0';
	bool com_initialized = false;
	try
	{
		int argument_count = 0;
		LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
		if (arguments == nullptr)
		{
			throw std::runtime_error("AudioSample could not read its command line.");
		}
		const bool smoke = argument_count == 2 &&
			std::wcscmp(arguments[1], L"--smoke-test") == 0;
		LocalFree(arguments);
		if (!smoke && argument_count != 1)
		{
			std::fprintf(stderr, "AudioSample arguments: %ls\n", command_line);
			throw std::invalid_argument("Usage: AudioSample.exe [--smoke-test]");
		}
		if (smoke)
		{
			if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
			{
				throw std::runtime_error("AudioSample could not initialize COM.");
			}
			com_initialized = true;
			audio_sample::smoke_test();
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
		return app.run(std::make_unique<audio_sample::AudioState>(&app));
	}
	catch (const std::exception& error)
	{
		if (com_initialized) { CoUninitialize(); }
		std::fprintf(stderr, "AudioSample: %s\n", error.what());
		if (interactive)
		{
			MessageBoxA(nullptr, error.what(), "AudioSample failure", MB_OK | MB_ICONERROR);
		}
		return 1;
	}
}

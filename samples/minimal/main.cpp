// The smallest thing that is still a game on this engine, and the file a new
// project starts from: copy samples/minimal/, rename the target, and own
// everything inside it.
//
// Everything here is a decision the engine cannot make for you - what the
// window is called, how big it is, where your content lives, and which of your
// states runs first. Everything it does not say is the engine's, and lives in
// Application (engine/app/application.h).

#include "engine/app/application.h"
#include "samples/minimal/states/hello_state.h"

#include <exception>
#include <cstdio>
#include <memory>
using namespace labrador;

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR,
	_In_ int show_command)
{
	try
	{
		ApplicationOptions options;
		options.window_class_name = L"MinimalSampleWindowClass";
		// ASCII only: this file gets copied into new projects, and a wide
		// literal's encoding depends on how the compiler was told to read the
		// source.
		options.window_title = L"Labrador - minimal sample";
		options.resolution = ScreenResolution::s_1280_720;

		// One pane, so one view's worth of recording state. The default is four
		// - four-player split-screen, the widest layout the engine has a client
		// for - and every view above what a frame draws is a deferred context
		// and a dynamic vertex buffer created at startup and never used.
		// A game that fans out says so here; this one does not.
		options.view_capacity = 1;

		Application app(std::move(options));
		app.initialize(instance, show_command);

		// Everything this sample draws, named in content/manifest.json. A game
		// with its own kinds of asset - levels, dialogue, whatever it has -
		// teaches them to app.resource_loader() before this line.
		app.load_manifest("./manifest.json");

		return app.run(std::make_unique<HelloState>(&app));
	}
	catch (const std::exception& e)
	{
		// A broken contract stops the program dead with the reason on screen,
		// never a silent abort (PHILOSOPHY T6).
		fprintf(stderr, "startup failure: %s\n", e.what());
		MessageBoxA(nullptr, e.what(), "Sample - startup failure",
			MB_OK | MB_ICONERROR);
		return 1;
	}
}

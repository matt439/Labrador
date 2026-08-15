// LineSweeper: a falling-block game, and the sample that shows what a whole
// game on this engine looks like.
//
// It is the second sample rather than a replacement for the first, because the
// two answer different questions. samples/minimal answers "how do I start a
// project on this engine" and is meant to be copied; this one answers "what
// does a finished game look like, and what does it cost on a small machine",
// and is meant to be read. ARCHITECTURE says which is which.

#include "engine/app/application.h"
#include "samples/linesweeper/states/play_state.h"

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
		options.window_class_name = L"LineSweeperWindowClass";
		// ASCII only, for the same reason samples/minimal is: a wide literal's
		// encoding depends on how the compiler was told to read the source.
		options.window_title = L"LineSweeper";
		options.resolution = ScreenResolution::s_1280_720;

		// One pane, so one view's worth of recording state. Every view above
		// what a frame draws is a deferred context and a dynamic vertex buffer
		// built at startup and never used, out of the same memory the game
		// runs in.
		options.view_capacity = 1;

		// PINNED, AND THE RULES DEPEND ON IT. Lock delay, the shift timer and
		// the gravity table are all denominated in fixed ticks, and this is
		// the number of ticks a second they assume. Nothing in the engine pins
		// it - validate() only checks it is positive - so a game that sets 120
		// here silently runs at half speed with no error anywhere.
		options.target_fps = 60;

		Application app(std::move(options));
		app.initialize(instance, show_command);
		app.load_manifest("./manifest.json");

		return app.run(std::make_unique<linesweeper::PlayState>(&app));
	}
	catch (const std::exception& e)
	{
		// A broken contract stops the program dead with the reason on screen,
		// never a silent abort (PHILOSOPHY T6).
		fprintf(stderr, "startup failure: %s\n", e.what());
		MessageBoxA(nullptr, e.what(), "LineSweeper - startup failure",
			MB_OK | MB_ICONERROR);
		return 1;
	}
}

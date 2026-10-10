// LineSweeper in a browser: main.cpp's twin for the web build, chosen in this
// folder's CMakeLists.txt so that neither file carries the other platform.
//
// The game is the same - the same options, the same first state, the same
// content, packed into the module rather than copied beside an executable -
// and two things differ, both of them the platform's. A failure has no
// message box to go to, so it goes to stderr, which is the browser console,
// and to the page's Module.onError if the page gave one. And the Application
// is a static rather than a local, because in a browser run() does not
// return (Application::run says why).

#include "engine/app/application.h"
#include "samples/linesweeper/states/play_state.h"

#include <emscripten/emscripten.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>
using namespace labrador;

EM_JS_DEPS(linesweeper_main, "$UTF8ToString");

EM_JS(void, linesweeper_tell_page_error, (const char* message), {
	if (typeof Module.onError === "function") {
		Module.onError(UTF8ToString(message));
	}
});

int main()
{
	try
	{
		// main.cpp's options, less the window class and title, which a
		// browser does not read: the page owns its title.
		ApplicationOptions options;
		options.resolution = ScreenResolution::s_1280_720;
		options.view_capacity = 1;

		// PINNED, AND THE RULES DEPEND ON IT, as main.cpp says at length:
		// every duration in the rules is a count of ticks at this rate.
		options.target_fps = 60;

		// NOT A LOCAL. run() hands the loop to the browser and unwinds this
		// stack before the first frame, destructors included, so the
		// Application has to outlive main. A static does, and is never
		// destroyed: closing the page takes the whole module with it.
		static Application app(std::move(options));
		app.initialize();

		// The content is packed into the root of the module's file system,
		// which is what a relative path resolves against in a browser
		// (engine/app/content_root.h).
		app.load_manifest("./manifest.json");

		return app.run(std::make_unique<linesweeper::PlayState>(&app));
	}
	catch (const std::exception& e)
	{
		// T6 on a web page: the reason, where the player can see it.
		std::fprintf(stderr, "startup failure: %s\n", e.what());
		linesweeper_tell_page_error(e.what());
		return 1;
	}
}

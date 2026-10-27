// LineSweeper in a browser: main.cpp's twin for the web build, chosen in this
// folder's CMakeLists.txt so that neither file carries the other platform.
//
// Ordinary play is the same - the same options, first state and
// content, packed into the module rather than copied beside an executable -
// and two things differ, both of them the platform's. A failure has no
// message box to go to, so it goes to stderr, which is the browser console,
// and to the page's Module.onError if the page gave one. And the Application
// is a static rather than a local, because in a browser run() does not
// return (Application::run says why).
//
// The page's prefers-reduced-motion is asked once, before the match, and a
// player who has set it gets a calmer particle field (particles.h, Motion).
//
// The development build alone includes an opt-in pacing observer and full-well
// fixture, enabled by its pre-js query flag. The packaged module compiles this
// entry without that code; default development play also starts PlayState.

#include "engine/app/application.h"
#include "samples/linesweeper/states/play_state.h"
#if defined(LABRADOR_DEVELOPMENT_PACING)
#include "samples/linesweeper/web/pacing_observer.h"
#include "samples/linesweeper/web/pacing_state.h"
#endif

#include <emscripten/emscripten.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>
using namespace labrador;

EM_JS_DEPS(linesweeper_main, "$UTF8ToString");

// The player's system setting, asked once at start: the field is made with
// it and keeps it for the page's life. The console line is what a page test
// reads to know which field it got.
EM_JS(int, linesweeper_prefers_reduced_motion, (), {
	const reduced = typeof matchMedia === "function" &&
		matchMedia("(prefers-reduced-motion: reduce)").matches;
	console.info("LineSweeper: motion " + (reduced ? "reduced" : "full") +
		" (prefers-reduced-motion)");
	return reduced ? 1 : 0;
});

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

#if defined(LABRADOR_DEVELOPMENT_PACING)
		const linesweeper::PacingMode pacing = linesweeper::pacing_mode();
		static linesweeper::PacingSnapshot pacing_snapshot;
		if (pacing != linesweeper::PacingMode::disabled)
		{
			linesweeper::observe_pacing(app, pacing_snapshot);
		}
		if (pacing == linesweeper::PacingMode::top_out ||
			pacing == linesweeper::PacingMode::top_out_frozen)
		{
			return app.run(std::make_unique<linesweeper::PacingState>(&app,
				&pacing_snapshot, pacing == linesweeper::PacingMode::top_out_frozen));
		}
#endif
		const linesweeper::Motion motion =
			linesweeper_prefers_reduced_motion() != 0 ?
			linesweeper::Motion::reduced : linesweeper::Motion::full;
		return app.run(std::make_unique<linesweeper::PlayState>(&app, motion));
	}
	catch (const std::exception& e)
	{
		// T6 on a web page: the reason, where the player can see it.
		std::fprintf(stderr, "startup failure: %s\n", e.what());
		linesweeper_tell_page_error(e.what());
		return 1;
	}
}

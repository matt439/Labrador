// The local multiplayer sample in a browser: main.cpp's twin for the web
// build, chosen in this folder's CMakeLists.txt so that neither file carries
// the other platform.
//
// The arena, both players and both views are the same files. Three things
// differ, and each is the platform's. A failure goes to stderr, which is the
// browser console, and to the page's Module.onError, because there is no
// message box. There is no --smoke-test, because a page has no command line,
// and LocalMultiplayerTests already checks the arena headlessly in this
// build. And the Application is a static rather than a local, because in a
// browser run() does not return (Application::run says why).

#include "engine/app/application.h"
#include "samples/local_multiplayer/play_state.h"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

using namespace labrador;

EM_JS_DEPS(local_multiplayer_main, "$UTF8ToString");

EM_JS(void, local_multiplayer_tell_page_error, (const char* message), {
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
		options.view_capacity = 2;
		// The same request main.cpp makes. A browser build's pool is serial
		// (core/thread_pool.h), so the two views still go to Scene::draw as
		// slices of work, and each slice runs inside add_task in turn.
		options.max_threads = std::min(2, default_thread_count());

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

		return app.run(std::make_unique<multiplayer::PlayState>(&app));
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "LocalMultiplayerSample: %s\n", e.what());
		local_multiplayer_tell_page_error(e.what());
		return 1;
	}
}

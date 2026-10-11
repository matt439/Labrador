// The collision sample in a browser: main.cpp's twin for the web build, chosen
// in this folder's CMakeLists.txt so that neither file carries the other
// platform.
//
// The arena, its bodies and the state that moves them are the same files.
// Three things differ, and each is the platform's. A failure goes to stderr,
// which is the browser console, and to the page's Module.onError, because
// there is no message box. There is no --smoke-test and no capture, because a
// page has no command line, and CollisionSampleTests already runs the arena
// headlessly in this build. And the Application is a static rather than a
// local, because in a browser run() does not return (Application::run says
// why).

#include "samples/collision/collision_state.h"

#include "engine/app/application.h"

#include <emscripten/emscripten.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

using namespace collisiondemo;
using namespace labrador;

EM_JS_DEPS(collision_main, "$UTF8ToString");

EM_JS(void, collision_tell_page_error, (const char* message), {
	if (typeof Module.onError === "function") {
		Module.onError(UTF8ToString(message));
	}
});

int main()
{
	try
	{
		// main.cpp's options, less the window class, the title and the
		// visibility, which a browser does not read: the page owns all three.
		ApplicationOptions options;
		options.view_capacity = 1;
		options.max_threads = 1;

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

		return app.run(std::make_unique<CollisionState>(&app, false));
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "CollisionSample: %s\n", error.what());
		collision_tell_page_error(error.what());
		return 1;
	}
}

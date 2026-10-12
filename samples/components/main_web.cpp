// The components gallery in a browser: main.cpp's twin for the web build,
// chosen in this folder's CMakeLists.txt so that neither file carries the
// other platform.
//
// What differs is the platform's, as in the collision sample's main_web.cpp:
// a failure goes to the console and to the page's Module.onError rather than
// to a message box, and the Application is a static rather than a local,
// because in a browser run() does not return (Application::run says why).

#include "samples/components/gallery_state.h"

#include "engine/app/application.h"

#include <emscripten/emscripten.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

using namespace components;
using namespace labrador;

EM_JS_DEPS(components_main, "$UTF8ToString");

EM_JS(void, components_tell_page_error, (const char* message), {
	if (typeof Module.onError === "function") {
		Module.onError(UTF8ToString(message));
	}
});

int main()
{
	try
	{
		// main.cpp's options, less the window class and the title, which the
		// page owns.
		ApplicationOptions options;
		options.view_capacity = 1;
		options.max_threads = 1;

		// NOT A LOCAL: run() unwinds this stack before the first frame.
		static Application app(std::move(options));
		app.initialize();
		app.load_manifest("./manifest.json");

		return app.run(std::make_unique<GalleryState>(&app));
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "ComponentsGallery: %s\n", error.what());
		components_tell_page_error(error.what());
		return 1;
	}
}

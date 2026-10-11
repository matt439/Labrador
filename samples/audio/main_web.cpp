// The audio sample in a browser: main.cpp's twin for the web build, chosen in
// this folder's CMakeLists.txt so that neither file carries the other
// platform.
//
// The sound board, its state and its content are the same files. Three things
// differ, and each is the platform's. A failure goes to stderr, which is the
// browser console, and to the page's Module.onError, because there is no
// message box. There is no --smoke-test, because a page has no command line:
// AudioSampleTests runs the same lifecycle against the same bank under Node in
// this build. And the Application is a static rather than a local, because in
// a browser run() does not return (Application::run says why).
//
// A page makes no sound before the person has pressed a key, clicked or
// touched it, and that is the Web Audio backend's to handle rather than this
// file's: engine/audio/audio_device.h states what a caller is told meanwhile.

#include "engine/app/application.h"
#include "samples/audio/audio_state.h"

#include <emscripten/emscripten.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

using namespace labrador;

EM_JS_DEPS(audio_sample_main, "$UTF8ToString");

EM_JS(void, audio_sample_tell_page_error, (const char* message), {
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

		return app.run(std::make_unique<audio_sample::AudioState>(&app));
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "AudioSample: %s\n", e.what());
		audio_sample_tell_page_error(e.what());
		return 1;
	}
}

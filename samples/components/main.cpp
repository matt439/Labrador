// The components gallery on Windows: one screen showing every component in
// this folder, so each can be seen and tried before it is copied.

#include "samples/components/gallery_state.h"

#include "engine/app/application.h"

#include <Windows.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

using namespace components;
using namespace labrador;

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	try
	{
		ApplicationOptions options;
		options.window_class_name = L"ComponentsGalleryWindow";
		options.window_title = L"Labrador - components";
		options.view_capacity = 1;
		options.max_threads = 1;

		Application app(std::move(options));
		app.initialize();
		app.load_manifest("./manifest.json");

		return app.run(std::make_unique<GalleryState>(&app));
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "ComponentsGallery: %s\n", error.what());
		MessageBoxA(nullptr, error.what(), "ComponentsGallery failure",
			MB_OK | MB_ICONERROR);
		return 1;
	}
}

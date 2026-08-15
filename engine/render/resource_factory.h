#pragma once

#include <string>

namespace artattack
{
	class Renderer;
	class RenderResources;

	// Builds the two resources that only exist on a device, and puts them in the
	// table.
	//
	// WHY THE DECLARATIONS ARE HERE AND THE BODIES ARE IN render/<backend>/.
	// Creating a texture from a file is a resource factory's job and not a
	// renderer's - renderer.h says so, and a Renderer that could read a file
	// would be a worse seam. But the factory is the one piece of that job whose
	// every line names a graphics type, so it belongs to the backend even though
	// none of its callers do.
	//
	// The bodies used to live in engine/assets/resource_loader.cpp, which made
	// assets/ the home of a backend translation unit. That was written down and
	// accepted. What was not written down is that it also put <d3d11_1.h> in
	// engine/assets/resource_loader.h - which engine/app/application.h includes,
	// which every state file in every client includes. The seam promises that
	// reaching for the device is "a deliberate include of
	// engine/render/<backend>/ and not something a game file can do by
	// accident"; transitively it already was. This is what closes it.
	//
	// Nothing declared here names a graphics type, so a caller compiles against
	// it with no backend header in scope. Each backend supplies its own
	// definitions, and asking for a backend that was not built is a missing
	// symbol at link rather than a failure at run time (T5).
	//
	// THE FILE EXTENSION IS THE BACKEND'S, WHICH IS WHY IT IS NOT A PARAMETER.
	// A kind owns its own file naming (resource_loader.h), and ".dds" and
	// ".spritefont" are not the engine's choices - they are what this backend
	// can decode. A backend that reads something else names something else, and
	// no caller changes.

	// Loads the texture named by `directory` and `name` onto the renderer's
	// device, and adds it to `resources` under `name`.
	//
	// Throws std::runtime_error naming the path if the file is not there or will
	// not decode (T6).
	void load_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name);

	// The same for a font, and it installs the stand-in glyph the backend draws
	// for a character the font does not have.
	//
	// Throws std::out_of_range naming the path if the font is not there.
	void load_font_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name);
}

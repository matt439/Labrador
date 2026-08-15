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
	// THE FILE EXTENSION IS NOT A PARAMETER, AND IT HAS STOPPED BEING THE
	// BACKEND'S. A kind owns its own file naming (resource_loader.h), and this
	// used to say that ".dds" and ".spritefont" were what this backend could
	// decode. They are not any more: both readers are engine/render/dds_file.*
	// and engine/render/sprite_font_file.*, so both extensions are the
	// engine's and every backend reads the same two files the same way.
	//
	// STILL OPEN, and worth doing when there is a second backend to measure it
	// against: each function below is now a path, a reader and one backend
	// call, so the only part a backend genuinely owns is the call. Having the
	// backend supply create_texture(renderer, TextureData) and moving these two
	// bodies out of engine/render/<backend>/ would halve what a port writes
	// here. It is not done yet because what a backend supplies is about to be
	// settled by a larger question - the sprite batch - and settling it twice
	// would be worse than settling it late.

	// Loads the texture named by `directory` and `name` onto the renderer's
	// device, and adds it to `resources` under `name`.
	//
	// Throws std::out_of_range naming the path if there is no file there, and
	// std::runtime_error naming the path and the problem if there is one and it
	// will not decode (T6). Those used to be one throw carrying an eight-digit
	// HRESULT, so "the artist has not exported it yet" and "the artist exported
	// a cube map" were the same message.
	void load_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name);

	// The same for a font, and it installs the stand-in glyph drawn for a
	// character the font does not have.
	//
	// It also adds the font's atlas to the texture table, under "font:<name>" -
	// a name no manifest can produce, because a colon cannot appear in a
	// filename. A font is engine data over a texture handle (font.h), and its
	// atlas has to be in the table like any other texture so that a device loss
	// empties it and a reload refills the same slot.
	void load_font_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name);
}

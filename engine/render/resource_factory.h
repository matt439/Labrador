#pragma once

#include <string>

namespace labrador
{
	class Renderer;
	class RenderResources;
	struct TextureData;

	// Builds the two resources that only exist on a device, and puts them in the
	// table.
	//
	// TWO OF THESE ARE THE ENGINE'S AND ONE IS THE BACKEND'S, and the line
	// between them is the point of the file. Loading a texture is: work out the
	// path, read the file, put the result in the table. Only the third step
	// names a graphics type. So the first two are written once, in
	// resource_factory.cpp, and a backend supplies add_texture_asset and
	// nothing else - which is thirty lines and the whole of what a port owes
	// for content.
	//
	// It was not always so. All of this lived in engine/assets/resource_loader.cpp,
	// which made assets/ the home of a backend translation unit - and put
	// <d3d11_1.h> in engine/assets/resource_loader.h, which engine/app/application.h
	// includes, which every state file in every client includes. The seam promises
	// that reaching for the device is "a deliberate include of
	// engine/render/<backend>/ and not something a game file can do by accident";
	// transitively it already was. Moving the whole factory to the backend closed
	// that, and this splits the factory again now that only one line of it is
	// actually the backend's.
	//
	// Nothing declared here names a graphics type, so a caller compiles against
	// it with no backend header in scope. Asking for a backend that was not built
	// is a missing symbol at link rather than a failure at run time (T5).
	//
	// THE FILE EXTENSION IS NOT A PARAMETER, AND IT IS NOT THE BACKEND'S. A kind
	// owns its own file naming (resource_loader.h), and this used to say ".dds"
	// and ".spritefont" were what a backend could decode. They are not: both
	// readers are engine/render/dds_file.* and engine/render/sprite_font_file.*,
	// so every backend reads the same two files the same way and neither
	// extension appears in engine/render/<backend>/ at all.

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

	// THE ONE LINE OF ALL THIS THAT A BACKEND WRITES. Makes a texture on the
	// renderer's device from bytes the engine has already decoded
	// (engine/render/texture_data.h), and puts it in the table under `name`.
	//
	// Every caller is one of the two above, and both of them are engine code -
	// so a client never sees this and a backend never sees a file. Throws
	// std::runtime_error naming `name` and the format if the device will not
	// take it, which is the answer a backend that cannot upload block
	// compression owes rather than a blank texture.
	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture);
}

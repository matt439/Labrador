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
	// nothing else - which is the whole of what a port owes for content, and
	// costs what the API charges: twenty-three lines on null, forty-seven on
	// d3d11 and eighty-six on gl, against a couple of hundred on each of the
	// two that own their own uploads. "Thirty lines" was written when the
	// smallest of them was the only one anybody had counted; renderer.h
	// measures all five in one place.
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
	//
	// AFTER create_device, AND THAT IS A RULE OF THE SEAM RATHER THAN ONE
	// BACKEND'S PREFERENCE. `renderer` is named here only because a texture is
	// made on a device, so there has to be one; a call before there is throws
	// std::runtime_error naming `name`. It was written down nowhere, and the
	// three backends answered it three ways - a named throw, an access
	// violation inside the D3D runtime, and, on the one configuration CI runs
	// end to end, a handle that resolved and drew. The ordering is fixed the
	// other way round from set_resources, which is why it is easy to get wrong:
	// the device comes first, then the table, then the content.
	//
	// AND NOT BETWEEN begin_frame AND submit, WHICH IS THE OTHER HALF OF THE
	// SAME RULE AND WAS MISSING FROM IT. Re-loading a name is ordinary and
	// supported - the table writes the new texture into the slot the name
	// already holds, so every handle resolved from it stays valid, and
	// RenderPixelTests pins that three hundred times over. What is not
	// supported is doing it while a frame is open. A recorded draw holds
	// whatever its backend calls a texture, taken at draw time and not owned:
	// an ID3D11ShaderResourceView*, a const D3d12Texture*, a GL texture name, a
	// const VulkanTexture* - and the four rasterising backends do not look at
	// it again until submit(), by which time the re-load has released it.
	// The null backend reads the texture at draw time and records a size, so it
	// answers differently, which is what makes this undefined rather than
	// merely dangerous: five backends, two behaviours, and no caller that could
	// tell them apart on purpose. Load content between frames.
	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture);
}

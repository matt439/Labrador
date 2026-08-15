#include "engine/render/resource_factory.h"

#include "engine/core/throw_if_failed.h"
#include "engine/render/d3d11/backend.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_font_file.h"
#include "engine/render/texture_format.h"

#include <DDSTextureLoader.h>
#include <wrl/client.h>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace artattack
{
	namespace
	{
		// Read fresh on every call rather than held.
		//
		// The loader used to cache the device and be told when it changed, because
		// a device restore hands back a different object and a stale pointer builds
		// resources on a dead one. That was a caller obligation - construct with
		// the device, and remember to re-seat it from on_device_restored - stated
		// in two places and enforced in none. Asking the renderer at the moment of
		// use is one member read, on the load path rather than the frame path, and
		// there is no ordering left to get wrong.
		ID3D11Device1* device_of(const Renderer& renderer)
		{
			return renderer.impl()->device_resources.GetD3DDevice();
		}

		// The engine's format vocabulary, back into this API's. The other
		// direction is in sprite_font_file.cpp, where a file that spells its
		// format as a DXGI number is decoded; this is the half that belongs to
		// a backend and it is three lines long.
		DXGI_FORMAT to_dxgi_format(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::b4g4r4a4_unorm:
				return DXGI_FORMAT_B4G4R4A4_UNORM;
			case TextureFormat::bc2_unorm:
				return DXGI_FORMAT_BC2_UNORM;
			case TextureFormat::r8g8b8a8_unorm:
			default:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}
		}

		// The atlas out of a .spritefont, as a texture like any other.
		//
		// IMMUTABLE AND SINGLE-MIP, which is what a font atlas is: it is
		// sampled at one scale by a batch that never mips, and nothing ever
		// writes to it. Those were DirectXTK's choices for the same texture and
		// they are kept, because this commit is about where the glyph table
		// lives and not about how its pixels are stored.
		ComPtr<ID3D11ShaderResourceView> create_atlas(ID3D11Device1* device,
			const SpriteFontFile& file)
		{
			D3D11_TEXTURE2D_DESC description = {};
			description.Width = static_cast<UINT>(file.width);
			description.Height = static_cast<UINT>(file.height);
			description.MipLevels = 1;
			description.ArraySize = 1;
			description.Format = to_dxgi_format(file.format);
			description.SampleDesc.Count = 1;
			description.Usage = D3D11_USAGE_IMMUTABLE;
			description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA initial = {};
			initial.pSysMem = file.pixels.data();
			initial.SysMemPitch = static_cast<UINT>(file.stride);
			initial.SysMemSlicePitch = static_cast<UINT>(file.pixels.size());

			ComPtr<ID3D11Texture2D> texture;
			ThrowIfFailed(device->CreateTexture2D(&description, &initial,
				texture.GetAddressOf()));

			ComPtr<ID3D11ShaderResourceView> view;
			ThrowIfFailed(device->CreateShaderResourceView(texture.Get(),
				nullptr, view.GetAddressOf()));
			return view;
		}

		// The stand-in glyph a font draws when asked for one it does not have.
		//
		// WITHOUT ONE, A MISSING GLYPH IS AN EXCEPTION. Font::drawn throws when
		// there is no stand-in, and U+0000 - "nobody chose" - is what every
		// .spritefont in this tree carries, because it is what MakeSpriteFont
		// writes by default, along with exactly 95 glyphs, U+0020 to U+007E.
		// Nothing chose that region and nothing records it, so the
		// out-of-the-box behaviour of the engine's own font kind was: any
		// character a text editor inserts on its own - a curly apostrophe, an
		// en-dash - kills the process on the frame that first drew it, which is
		// typically several screens from wherever the string was read.
		//
		// The engine already documented the opposite. text_encoding.h promises
		// that invalid UTF-8 becomes U+FFFD so that "text about to be drawn should
		// show mojibake, not vanish" - and U+FFFD is outside 32..126, so the
		// documented graceful path led directly into the throw it was written to
		// avoid. It leads to a question mark now.
		//
		// Degradation rather than a throw is also what the audio half of the
		// loader already does: a missing optional wave bank becomes a bank that
		// plays nothing rather than a dead process.
		//
		// '?' FIRST, ' ' AFTER IT, and nothing if the font has neither. A question
		// mark says "something was here and could not be drawn", which is the
		// mojibake text_encoding.h asks for; a space says only that the text is
		// oddly spaced, and is the better of the two only when there is no
		// question mark to be had. A font with neither is not a text font - an
		// icon atlas, say - and is left exactly as it was rather than warned
		// about, because there is nothing wrong with it.
		//
		// THE FILE'S OWN CHOICE IS HONOURED FIRST, which it never was before:
		// a .spritefont says which character it would like, and SpriteFont read
		// that and then this overwrote it. It is U+0000 in every font here, so
		// nothing in this tree changes; a font built by somebody who did choose
		// now gets what they chose.
		void install_stand_in(Font& font, char32_t chosen)
		{
			if (chosen != 0 && font.contains(chosen))
			{
				font.set_stand_in(chosen);
				return;
			}
			if (font.contains(U'?'))
			{
				font.set_stand_in(U'?');
				return;
			}
			if (font.contains(U' '))
			{
				font.set_stand_in(U' ');
			}
		}
	}

	void load_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name)
	{
		const std::string texture_path = directory + name + ".dds";

		ComPtr<ID3D11ShaderResourceView> texture_view;
		ComPtr<ID3D11Resource> resource;
		const HRESULT result = CreateDDSTextureFromFile(device_of(renderer),
			std::wstring(texture_path.begin(), texture_path.end()).c_str(),
			resource.GetAddressOf(),
			texture_view.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			// ThrowIfFailed's message is the HRESULT and nothing else, so the
			// commonest failure here - a texture named in the manifest that is
			// not on disk - arrived as eight hex digits. The font loader below
			// has always named its file; this now does too (T6).
			char hresult[16] = {};
			std::snprintf(hresult, sizeof(hresult), "0x%08X",
				static_cast<unsigned int>(result));
			throw std::runtime_error("Failed to load texture: " + texture_path +
				" (" + hresult + ")");
		}

		resources.impl()->add_texture(name, texture_view.Get());
	}

	void load_font_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& directory,
		const std::string& name)
	{
		const std::string font_path = directory + name + ".spritefont";

		// Reading the file is not this backend's job and no longer happens
		// here; two of the four things it produces are engine data and one of
		// them is the arithmetic that places every glyph.
		const SpriteFontFile file = read_sprite_font_file(font_path);

		// THE ATLAS GOES IN THE TEXTURE TABLE, UNDER A NAME NO MANIFEST CAN
		// PRODUCE. It has to be in there rather than held by the Font: the
		// table is what a device loss empties and a reload refills, and a
		// texture the table has never heard of would survive a loss as a
		// dangling pointer inside a font that nothing thinks is a device
		// resource. The colon is what makes the name safe - it cannot appear in
		// a Windows filename, and every other name in this table is one.
		const std::string atlas_name = "font:" + name;
		resources.impl()->add_texture(atlas_name,
			create_atlas(device_of(renderer), file).Get());

		std::unique_ptr<Font> font = std::make_unique<Font>(
			resources.resolve_texture(atlas_name), file.glyphs,
			file.line_spacing);
		install_stand_in(*font, file.stand_in);

		resources.add_font(name, std::move(font));
	}
}

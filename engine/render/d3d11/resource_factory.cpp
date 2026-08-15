#include "engine/render/resource_factory.h"

#include "engine/core/throw_if_failed.h"
#include "engine/render/d3d11/backend.h"
#include "engine/render/dds_file.h"
#include "engine/render/font.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_font_file.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <wrl/client.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
		// direction is in dds_file.cpp and sprite_font_file.cpp, where files
		// that spell their format as a fourCC or a DXGI number are decoded;
		// this is the half that belongs to a backend and it is a switch.
		DXGI_FORMAT to_dxgi_format(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::b8g8r8a8_unorm:
				return DXGI_FORMAT_B8G8R8A8_UNORM;
			case TextureFormat::b4g4r4a4_unorm:
				return DXGI_FORMAT_B4G4R4A4_UNORM;
			case TextureFormat::bc1_unorm:
				return DXGI_FORMAT_BC1_UNORM;
			case TextureFormat::bc2_unorm:
				return DXGI_FORMAT_BC2_UNORM;
			case TextureFormat::bc3_unorm:
				return DXGI_FORMAT_BC3_UNORM;
			case TextureFormat::r8g8b8a8_unorm:
			default:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}
		}

		// Bytes into a texture, and this is the whole of what a backend does
		// with a file now.
		//
		// ONE FUNCTION FOR BOTH FILE KINDS, which is what sharing TextureData
		// between the two readers bought: a .dds and the atlas inside a
		// .spritefont differ in how they are decoded and not at all in what
		// comes out, so there is nothing here that knows which one it is
		// looking at.
		//
		// IMMUTABLE, because nothing ever writes to one of these, and that is
		// what DirectXTK asked for on the same textures.
		ComPtr<ID3D11ShaderResourceView> create_texture(ID3D11Device1* device,
			const TextureData& data)
		{
			D3D11_TEXTURE2D_DESC description = {};
			description.Width = static_cast<UINT>(data.width);
			description.Height = static_cast<UINT>(data.height);
			description.MipLevels = static_cast<UINT>(data.levels.size());
			description.ArraySize = 1;
			description.Format = to_dxgi_format(data.format);
			description.SampleDesc.Count = 1;
			description.Usage = D3D11_USAGE_IMMUTABLE;
			description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			std::vector<D3D11_SUBRESOURCE_DATA> initial(data.levels.size());
			for (size_t i = 0; i < data.levels.size(); i++)
			{
				const TextureLevel& level = data.levels[i];
				initial[i].pSysMem = data.pixels.data() + level.offset;
				initial[i].SysMemPitch = static_cast<UINT>(level.stride);
				initial[i].SysMemSlicePitch = static_cast<UINT>(level.size);
			}

			ComPtr<ID3D11Texture2D> texture;
			ThrowIfFailed(device->CreateTexture2D(&description, initial.data(),
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

		// Reading the file is not this backend's job and no longer happens
		// here. What used to be one CreateDDSTextureFromFile call was a file
		// reader, a format table and a device texture in a library only this
		// backend can link, and the first two are neither this backend's nor
		// DirectX's - they are what this engine's content is.
		resources.impl()->add_texture(name,
			create_texture(device_of(renderer),
				read_dds_file(texture_path)).Get());
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
			create_texture(device_of(renderer), file.atlas).Get());

		std::unique_ptr<Font> font = std::make_unique<Font>(
			resources.resolve_texture(atlas_name), file.glyphs,
			file.line_spacing);
		install_stand_in(*font, file.stand_in);

		resources.add_font(name, std::move(font));
	}
}

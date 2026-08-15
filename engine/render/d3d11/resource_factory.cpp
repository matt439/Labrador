#include "engine/render/resource_factory.h"

#include "engine/render/d3d11/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"

#include <DDSTextureLoader.h>
#include <wrl/client.h>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

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

		// The stand-in glyph a font draws when asked for one it does not have.
		//
		// WITHOUT ONE, A MISSING GLYPH IS AN EXCEPTION. DirectXTK's FindGlyph
		// throws when the default character is U+0000, and U+0000 is what every
		// .spritefont in this tree carries, because it is what MakeSpriteFont
		// writes when nobody chooses - along with exactly 95 glyphs, U+0020 to
		// U+007E. Nothing chose that region and nothing records it, so the
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
		// Asked before it is set, never caught: SetDefaultCharacter throws for a
		// character the font does not have, and that is the throw this whole
		// function exists to stop happening.
		void install_default_character(DirectX::SpriteFont& font)
		{
			if (font.ContainsCharacter(L'?'))
			{
				font.SetDefaultCharacter(L'?');
				return;
			}
			if (font.ContainsCharacter(L' '))
			{
				font.SetDefaultCharacter(L' ');
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
			// not on disk - arrived as eight hex digits. The font loader twenty
			// lines below has always named its file; this now does too (T6).
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

		std::unique_ptr<SpriteFont> font;
		try
		{
			font = std::make_unique<SpriteFont>(device_of(renderer),
				std::wstring(font_path.begin(), font_path.end()).c_str());
		}
		catch (const std::exception&)
		{
			// DirectXTK's what() does not say which font or where - T6 does.
			throw std::out_of_range(
				"SpriteFont " + name + " not found at " + font_path + ".");
		}

		install_default_character(*font);
		resources.impl()->add_sprite_font(name, std::move(font));
	}
}

#include "engine/render/resource_factory.h"

#include "engine/render/d3d11/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/render/throw_if_failed.h"

#include <wrl/client.h>

#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace labrador
{
	namespace
	{
		// Read fresh on every call rather than held.
		//
		// The loader does not cache the device. A restore hands back a different
		// object and a stale pointer builds resources on a dead one, so caching
		// means a caller obligation - construct with the device, and remember to
		// re-seat it from on_device_restored - of the kind stated in two places
		// and enforced in none. Asking the renderer at the moment of use is one
		// member read, on the load path rather than the frame path, and there is
		// no ordering to get wrong.
		ID3D11Device1* device_of(const Renderer& renderer)
		{
			return renderer.impl()->device_resources.GetD3DDevice();
		}

		// The engine's format vocabulary, back into this API's. The other
		// direction is in dds_file.cpp and sprite_font_file.cpp, where files
		// that spell their format as a fourCC or a DXGI number are decoded;
		// this is the half that belongs to a backend and it is a switch.
		DXGI_FORMAT to_dxgi_format(TextureFormat format,
			const std::string& name)
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
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}

			// Every enumerator returns above. A value outside the enum is
			// refused by name, as resource_factory.h promises, rather than
			// uploaded as some other format's bytes.
			throw std::runtime_error("Texture '" + name + "' is in a format "
				"this backend has no Direct3D 11 format for (TextureFormat " +
				std::to_string(static_cast<int>(format)) + ").");
		}

		// For the throw, because an eight-digit HRESULT is not an answer (T6).
		std::string format_name(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::b8g8r8a8_unorm: return "b8g8r8a8_unorm";
			case TextureFormat::b4g4r4a4_unorm: return "b4g4r4a4_unorm";
			case TextureFormat::bc1_unorm:      return "bc1_unorm";
			case TextureFormat::bc2_unorm:      return "bc2_unorm";
			case TextureFormat::bc3_unorm:      return "bc3_unorm";
			case TextureFormat::r8g8b8a8_unorm: return "r8g8b8a8_unorm";
			}
			return "TextureFormat " + std::to_string(static_cast<int>(format));
		}

		// THE ANSWER resource_factory.h PROMISES WHEN THE DEVICE WILL NOT TAKE
		// A TEXTURE: its name, its format and size, and the code. The likeliest
		// format to arrive here is b4g4r4a4_unorm, which Direct3D 11 makes
		// optional. The HRESULT stays in the message rather than being read as
		// a cause, because E_OUTOFMEMORY and a removed device fail here too.
		[[noreturn]] void refuse(const std::string& name,
			const TextureData& texture, const char* step, HRESULT hr)
		{
			throw std::runtime_error("Texture '" + name + "' is " +
				format_name(texture.format) + " at " +
				std::to_string(texture.width) + "x" +
				std::to_string(texture.height) + ", and this device would "
				"not create its " + step + " (" + hresult_name(hr) + ").");
		}
	}

	// The whole of this backend's share of loading content, and it does not
	// know which file the bytes came from - a .dds and the atlas inside a
	// .spritefont differ in how they are decoded and not at all in what comes
	// out (engine/render/texture_data.h).
	//
	// IMMUTABLE, because nothing ever writes to one of these, and that is what
	// DirectXTK asked for on the same textures.
	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture)
	{
		// BEFORE ANY OF THE DESCRIPTION BELOW, because device_of is a bare
		// GetD3DDevice() and it is null until create_device has run. The two
		// calls that use it check their HRESULTs, which reads like a guard and
		// is not one: the dereference happens first, so a load before the
		// device existed was an access violation inside the D3D runtime with
		// nothing catchable and no name in it. The seam states the ordering
		// (resource_factory.h); this is one of the FIVE places it is kept, one
		// per backend, each throwing a runtime_error that names the texture.
		if (device_of(renderer) == nullptr)
		{
			throw std::runtime_error("Texture '" + name + "' was loaded before "
				"create_device made a device.");
		}

		D3D11_TEXTURE2D_DESC description = {};
		description.Width = static_cast<UINT>(texture.width);
		description.Height = static_cast<UINT>(texture.height);
		description.MipLevels = static_cast<UINT>(texture.levels.size());
		description.ArraySize = 1;
		description.Format = to_dxgi_format(texture.format, name);
		description.SampleDesc.Count = 1;
		description.Usage = D3D11_USAGE_IMMUTABLE;
		description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		std::vector<D3D11_SUBRESOURCE_DATA> initial(texture.levels.size());
		for (size_t i = 0; i < texture.levels.size(); i++)
		{
			const TextureLevel& level = texture.levels[i];
			initial[i].pSysMem = texture.pixels.data() + level.offset;
			initial[i].SysMemPitch = static_cast<UINT>(level.stride);
			initial[i].SysMemSlicePitch = static_cast<UINT>(level.size);
		}

		ComPtr<ID3D11Texture2D> texture_2d;
		const HRESULT created = device_of(renderer)->CreateTexture2D(
			&description, initial.data(), texture_2d.GetAddressOf());
		if (FAILED(created))
		{
			refuse(name, texture, "texture", created);
		}

		ComPtr<ID3D11ShaderResourceView> view;
		const HRESULT viewed = device_of(renderer)->CreateShaderResourceView(
			texture_2d.Get(), nullptr, view.GetAddressOf());
		if (FAILED(viewed))
		{
			refuse(name, texture, "shader resource view", viewed);
		}

		resources.impl()->add_texture(name, view.Get());
	}
}

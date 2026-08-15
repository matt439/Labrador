#include "engine/render/resource_factory.h"

#include "engine/core/throw_if_failed.h"
#include "engine/render/d3d11/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <wrl/client.h>

#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace labrador
{
	namespace
	{
		// Read fresh on every call rather than held.
		//
		// The loader used to cache the device and be told when it changed,
		// because a device restore hands back a different object and a stale
		// pointer builds resources on a dead one. That was a caller obligation -
		// construct with the device, and remember to re-seat it from
		// on_device_restored - stated in two places and enforced in none. Asking
		// the renderer at the moment of use is one member read, on the load path
		// rather than the frame path, and there is no ordering left to get wrong.
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
		D3D11_TEXTURE2D_DESC description = {};
		description.Width = static_cast<UINT>(texture.width);
		description.Height = static_cast<UINT>(texture.height);
		description.MipLevels = static_cast<UINT>(texture.levels.size());
		description.ArraySize = 1;
		description.Format = to_dxgi_format(texture.format);
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
		ThrowIfFailed(device_of(renderer)->CreateTexture2D(&description,
			initial.data(), texture_2d.GetAddressOf()));

		ComPtr<ID3D11ShaderResourceView> view;
		ThrowIfFailed(device_of(renderer)->CreateShaderResourceView(
			texture_2d.Get(), nullptr, view.GetAddressOf()));

		resources.impl()->add_texture(name, view.Get());
	}
}

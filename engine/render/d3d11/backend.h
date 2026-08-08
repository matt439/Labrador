#pragma once

#include "engine/core/registry.h"
#include "engine/render/d3d11/device_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"

#include <CommonStates.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <wrl/client.h>

#include <memory>
#include <vector>
#include "engine/render/camera.h"

// The D3D11 backend, for the two callers that have to name it.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include. It has exactly two clients:
//
//   - engine/app/application.cpp, which needs an HWND and a swap chain, and
//   - engine/assets/resource_loader.cpp, which builds textures and fonts on a
//     device and puts them in the table.
//
// A third would be a mistake. Everything that draws goes through DrawList, and
// the whole point of the seam is that draw code never learns which backend it
// is talking to.

namespace artattack
{
	// COM spells the accessor Get(), so teach the registry to unwrap a ComPtr
	// here, where COM is already in scope.
	template <typename Resource>
	struct RegistryStorage<Microsoft::WRL::ComPtr<Resource>>
	{
		static Resource* pointer(const Microsoft::WRL::ComPtr<Resource>& storage)
		{
			return storage.Get();
		}
	};

	// What a Texture and a Font are, once this backend has decided. The phantom
	// types in renderer.h never gain a definition; the handles that name them
	// index the tables below, and this is where the index becomes a resource.
	class RenderResources::Impl
	{
	public:
		// Load-time, and the reason this header exists: everything here is
		// spelt in DirectXTK and D3D11 types, which is exactly what the public
		// RenderResources may not say.
		void add_texture(const std::string& name,
			ID3D11ShaderResourceView* texture);
		void add_sprite_font(const std::string& name,
			std::unique_ptr<DirectX::SpriteFont> font);
		void add_sprite_sheet(const std::string& name,
			std::unique_ptr<SpriteSheet> sprite_sheet);

		// Device loss. The resources go; the names, and therefore every handle
		// resolved from them, stay.
		void release_all_textures();
		void release_all_sprite_fonts();

		// Per-draw, from DrawList. Each throws std::out_of_range if the handle
		// is unresolved or its slot has been released.
		ID3D11ShaderResourceView* texture(TextureHandle texture) const;
		DirectX::SpriteFont* sprite_font(FontHandle font) const;

		// By name, for the loader that has just created one and wants it back.
		ID3D11ShaderResourceView* texture(const std::string& name) const;

		Registry<ID3D11ShaderResourceView,
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
			textures{ "Texture" };
		Registry<DirectX::SpriteFont> sprite_fonts{ "SpriteFont" };
		Registry<SpriteSheet> sprite_sheets{ "SpriteSheet" };
	};

	// One view's recording state: a deferred context and the sprite batch that
	// writes into it, plus the three things set_viewport / set_camera /
	// set_filter remember between draws.
	//
	// DrawList holds a pointer to one of these and nothing else, which is what
	// makes a DrawList trivially copyable and free to pass.
	class DrawList::View
	{
	public:
		Renderer::Impl* owner = nullptr;
		ID3D11DeviceContext* context = nullptr;
		std::unique_ptr<DirectX::SpriteBatch> batch;

		// Reset at the top of every frame, not carried across one.
		Camera camera = Camera::DEFAULT_CAMERA;
		TextureFilter filter = TextureFilter::point;
		bool batch_open = false;

		// Whether anything has been recorded into this view this frame. It is
		// what makes a dropped view detectable: a caller that declares four
		// views, fills them, and then declares two has stranded two command
		// lists in their contexts, and they would surface in the *next*
		// frame's list. See Renderer::set_view_count.
		bool touched = false;

		// A sprite batch cannot change its sampler mid-Begin, so a filter
		// change is a flush and a reopen. Opening is deferred to the first
		// draw so that a view nobody drew into never opens one at all.
		void open_batch();
		void close_batch();

		void reset();
		ID3D11CommandList* finish();
	};

	// The device, the swap chain, the views, and the sampler states - which is
	// to say every D3D11 object with a lifetime longer than one draw.
	//
	// It implements D3DDeviceNotify itself and forwards to the seam's
	// DeviceNotify, so the game hears about a device loss without ever hearing
	// what a device is.
	class Renderer::Impl final : public D3DDeviceNotify
	{
	public:
		Impl();

		// Renders only 2D, so no depth buffer.
		DeviceResources device_resources{ DXGI_FORMAT_B8G8R8A8_UNORM,
			DXGI_FORMAT_UNKNOWN };

		std::vector<std::unique_ptr<DrawList::View>> views;
		int view_count = 0;

		std::unique_ptr<DirectX::CommonStates> states;

		const RenderResources* resources = nullptr;
		artattack::DeviceNotify* notify = nullptr;

		ID3D11SamplerState* sampler(TextureFilter filter) const;

		// The sprite batches and the sampler states, which belong to the
		// device and are remade with it.
		void create_device_dependent_resources();

		void OnDeviceLost() override;
		void OnDeviceRestored() override;
	};
}

#pragma once

#include "engine/core/registry.h"
#include "engine/render/d3d11/device_resources.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"

#include <CommonStates.h>
#include <SpriteBatch.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>
#include "engine/render/camera.h"

// The D3D11 backend, for the two callers that have to name it.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include. It has exactly two clients:
//
//   - engine/app/application.cpp, which needs an HWND and a swap chain, and
//   - engine/render/d3d11/resource_factory.cpp, which builds textures on a
//     device and puts them in the table - including the atlas a font is cut
//     from, which is the one part of loading a font that needs one.
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

	// What a Texture is, once this backend has decided, and where every named
	// resource lives. The phantom type in renderer.h never gains a definition;
	// the handles that name it index the table below, and this is where the
	// index becomes a resource.
	//
	// ONE OF THE THREE TABLES IS THIS BACKEND'S AND TWO ARE NOT. Fonts and
	// sheets are engine data (font.h, sprite_sheet.h) and are here only because
	// the storage of a pimpl is the pimpl's; the textures are the sole reason
	// this class cannot be written once for everybody. That is a smaller
	// difference than it was - a font used to be a DirectX::SpriteFont, which
	// is to say the pen arithmetic for every string this engine draws used to
	// be in this table - and it is the shape a second backend inherits.
	class RenderResources::Impl
	{
	public:
		// Load-time, and the reason this header exists: this is spelt in a D3D11
		// type, which is exactly what the public RenderResources may not say.
		// The adds for the two engine kinds are on RenderResources itself.
		void add_texture(const std::string& name,
			ID3D11ShaderResourceView* texture);
		void add_font(const std::string& name, std::unique_ptr<Font> font);
		void add_sprite_sheet(const std::string& name,
			std::unique_ptr<SpriteSheet> sprite_sheet);

		// Device loss. The textures go; the names, and therefore every handle
		// resolved from them, stay. Nothing else in here is the device's.
		void release_all_textures();

		// Per-draw, from DrawList. Each throws std::out_of_range if the handle
		// is unresolved or its slot has been released.
		ID3D11ShaderResourceView* texture(TextureHandle texture) const;
		const Font* font(FontHandle font) const;

		// By name, for the loader that has just created one and wants it back.
		ID3D11ShaderResourceView* texture(const std::string& name) const;

		Registry<ID3D11ShaderResourceView,
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
			textures{ "Texture" };
		Registry<Font> fonts{ "Font" };
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
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
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

		// Whether this frame's render target and viewport have been bound into
		// this view's deferred context.
		//
		// A deferred context holds every command recorded into it until
		// FinishCommandList takes them away, and binding is itself two recorded
		// commands. So a context that is bound and never finished accumulates
		// them for the life of the process - which is exactly what happened
		// while begin_frame bound all sixteen and submit() finished the one the
		// frame declared: 1,800 stranded commands a second, on the default
		// configuration, growing without bound.
		//
		// Binding is therefore deferred to set_view_count, which is the first
		// moment anyone knows which views this frame has, and submit() finishes
		// every view that says yes here rather than every view the frame
		// declared. The two sets differ when a caller lowers the count.
		bool bound = false;

		// A sprite batch cannot change its sampler mid-Begin, so a filter
		// change is a flush and a reopen. Opening is deferred to the first
		// draw so that a view nobody drew into never opens one at all.
		void open_batch();
		void close_batch();

		// Records this frame's render target and viewport into the context, and
		// tells the batch what to project against. Idempotent within a frame:
		// the second call on a bound view does nothing, so a caller that
		// declares its view count twice does not re-bind a viewport over the one
		// set_viewport just chose.
		void bind(ID3D11RenderTargetView* render_target,
			const D3D11_VIEWPORT& viewport);

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

		// Renders only 2D, so there is no depth buffer to ask for - see the
		// class comment on DeviceResources, which no longer has one to offer.
		DeviceResources device_resources{ DXGI_FORMAT_B8G8R8A8_UNORM };

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

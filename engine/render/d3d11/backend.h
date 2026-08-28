#pragma once

#include "engine/core/registry.h"
#include "engine/render/d3d11/device_resources.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_vertex.h"

#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>
#include "engine/render/camera.h"

// The D3D11 backend, for the files that have to name a device.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include.
//
// EVERY CLIENT OF IT IS IN THIS FOLDER, and that is the rule rather than a
// count. cmake/check_engine_includes.cmake fails the build for any file outside
// engine/render/<backend>/ that includes any header inside it - the folder, not
// one filename in it, because device_resources.h was how the wall was climbed
// last time. The three .cpp beside this file include it; nothing else in the
// tree may, and nothing else does - the two generated shader headers are fxc
// /Fh output, which is a comment and a byte array with no #include in it. This paragraph used to name its clients
// individually and the list went stale twice: engine/app/application.cpp is on
// the far side of the wall now and hands its window over as a void*, and
// engine/render/d3d11/resource_factory.cpp never existed under that name.
// Everything that draws goes through DrawList, and the whole point of the seam
// is that draw code never learns which backend it is talking to.

namespace labrador
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
		// The adds for the two engine kinds are on RenderResources itself, and
		// so are their tables now - this holds one table, not three.
		void add_texture(const std::string& name,
			ID3D11ShaderResourceView* texture);

		// Device loss. The textures go; the names, and therefore every handle
		// resolved from them, stay. Nothing else in here is the device's, and
		// there is nothing else in here.
		void release_all_textures();

		// Per-draw, from DrawList. Throws std::out_of_range if the handle is
		// unresolved or its slot has been released. The font lookup that used
		// to sit beside this one is RenderResources::font: a Font is engine
		// data, so its table was never hiding anything from anybody.
		ID3D11ShaderResourceView* texture(TextureHandle texture) const;

		// By name. NOT FOR THE LOADER, which is what this used to say: the load
		// path calls add_texture and then asks the seam for a handle
		// (RenderResources::resolve_texture), because a handle is what a
		// drawable holds. This overload is the by-name lookup the Registry
		// offers, kept for parity with the other four backends' Impls.
		ID3D11ShaderResourceView* texture(const std::string& name) const;

		Registry<ID3D11ShaderResourceView,
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
			textures{ "Texture" };
	};

	// One view's recording state: a deferred context, the buffers that feed it,
	// and the three things set_viewport / set_camera / set_filter remember
	// between draws.
	//
	// DrawList holds a pointer to one of these and nothing else, which is what
	// makes a DrawList trivially copyable and free to pass.
	//
	// THE BUFFERS ARE PER VIEW BECAUSE THE CONTEXT IS. Several workers record
	// at once, into their own deferred contexts, and a dynamic buffer being
	// mapped by two of them at the same time is not a race that shows up as
	// anything readable. One vertex buffer and one constant buffer each; the
	// index buffer, the shaders and the states are shared, because nothing ever
	// writes to those.
	class DrawList::View
	{
	public:
		// How many sprites one vertex buffer holds. DirectXTK's number, and
		// arrived at the same way: 16-bit indices cap it at 16384 sprites, and
		// a buffer of 2048 is 256KB of vertices per view - which is affordable
		// on the low tier and large enough that the fill-and-flush path is not
		// what a frame is spending its time on. PHILOSOPHY.md, Performance,
		// names the low tier this is affordable on, and this is the half of
		// that term a written spec settles: 256KB is a memory budget.
		static const int MAX_BATCH_SPRITES = 2048;

		Renderer::Impl* owner = nullptr;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

		// Dynamic, MAX_BATCH_SPRITES * 4 vertices. Written by Map, never by
		// UpdateSubresource: a deferred context records the copy either way,
		// but DISCARD is what lets the driver hand back fresh memory instead of
		// waiting for the last frame's draw to finish reading this one.
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertices;

		// The pixels-to-clip transform the vertex shader multiplies by. Per
		// view because a view has its own viewport, and set_viewport is allowed
		// to change it mid-list.
		Microsoft::WRL::ComPtr<ID3D11Buffer> transform;

		// Where the next batch is written in `vertices`, in sprites. Wraps to
		// zero when a batch will not fit, which is the moment DISCARD is asked
		// for; anything else is a NO_OVERWRITE append.
		int buffer_position = 0;

		// The sprites recorded since the last flush, four corners each. Built
		// on the CPU and copied in one go, because a Map per sprite is a
		// round trip per sprite.
		std::vector<SpriteVertex> batch;

		// What `batch` is drawn with. A change to either is what a flush is
		// for: one texture and one sampler per draw call.
		ID3D11ShaderResourceView* batch_texture = nullptr;

		// Reset at the top of every frame, not carried across one.
		Camera camera = Camera::DEFAULT_CAMERA;
		TextureFilter filter = TextureFilter::point;

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

		// Appends one sprite's four corners, flushing first if it cannot join
		// what is already there - a different texture, or a full buffer.
		void draw(ID3D11ShaderResourceView* texture,
			const SpriteVertex* corners);

		// Records one draw call for everything appended since the last flush,
		// and nothing at all when nothing has been. Called on a texture change,
		// on a filter or viewport change, and at the end of the view.
		void flush();

		// Records this frame's render target and viewport into the context, and
		// writes the transform they imply. Idempotent within a frame: the second
		// call on a bound view does nothing, so a caller that declares its view
		// count twice does not re-bind a viewport over the one set_viewport just
		// chose.
		void bind(ID3D11RenderTargetView* render_target,
			const D3D11_VIEWPORT& viewport);

		// Pixels to clip space for this viewport, into the constant buffer.
		void set_transform(const D3D11_VIEWPORT& viewport);

		void reset();

		// Flushes what is batched, takes the command list off the context and
		// clears `bound`, because FinishCommandList(FALSE) leaves the context
		// with none of the state bind() recorded into it. The caller owns the
		// list and releases it.
		ID3D11CommandList* finish();

		// Takes the command list off the context and throws it away.
		//
		// FOR A FRAME THAT WAS BEGUN AND NEVER SUBMITTED, which is the one
		// state where this backend could differ from the other four about what a
		// frame is. On three of those four - gl, null and vulkan - a view's
		// recording is a std::vector that reset() empties; on the fourth it is
		// a command list that d3d12 closes without executing; here it is a deferred context, which holds what has been
		// recorded into it until something takes it away - so clearing `bound`
		// without draining first left an abandoned frame's already-flushed
		// geometry to arrive prepended to the next frame's command list, over
		// the top of the next frame's clear. Does nothing on a view that is not
		// bound, so the normal path - where submit() has just drained
		// everything - costs one branch per view per frame.
		void discard();
	};

	// The device, the swap chain, the views, and everything a draw call needs
	// that is the same for every draw call - which is to say every D3D11 object
	// with a lifetime longer than one frame.
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

		// BETWEEN begin_frame AND end_frame, WHICH IS A WIDER INTERVAL THAN
		// "SOME VIEW IS BOUND" - and it is the wider one renderer.h
		// legislates. No view is bound between begin_frame and the first
		// set_view_count, because binding is set_view_count's job and it has
		// not been called yet. A resize arriving in that window still has a
		// frame to restart, and asking the views whether there is one answers
		// no - which cost that frame its clear. See
		// Renderer::window_size_changed; the D3D12 backend tracks the same
		// term for the same reason and a worse consequence.
		bool frame_begun = false;

		// Whether submit() has already run for this frame. renderer.h makes a
		// second submit a no-op; this is the whole of what that costs.
		bool frame_submitted = false;

		// SHARED BY EVERY VIEW, because nothing writes to any of them. The
		// index buffer is the one that would surprise somebody: it is the same
		// two triangles per sprite for every sprite there will ever be, so it
		// is filled once at device creation and never touched again.
		Microsoft::WRL::ComPtr<ID3D11Buffer> indices;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;

		// The five state objects a sprite pass needs, and there are only five
		// because it needs no more: premultiplied alpha, no depth at all, no
		// culling, and one sampler for each filter the seam names. This
		// replaced DirectX::CommonStates, which offered twenty-odd and was
		// asked for four.
		Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> point_sampler;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> linear_sampler;

		const RenderResources* resources = nullptr;
		labrador::DeviceNotify* notify = nullptr;

		ID3D11SamplerState* sampler(TextureFilter filter) const;

		// The size of a texture, in texels, for the geometry that turns a
		// source rectangle into texture coordinates. Read off the view rather
		// than remembered beside it, because the table stores views.
		static mattmath::Vector2F texture_size(
			ID3D11ShaderResourceView* texture);

		// Everything above that belongs to the device, remade with it.
		void create_device_dependent_resources();

		void OnDeviceLost() override;
		void OnDeviceRestored() override;
	};
}

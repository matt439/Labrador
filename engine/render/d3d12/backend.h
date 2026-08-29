#pragma once

#include "engine/core/registry.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/d3d12/device_resources.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_vertex.h"

#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

// The Direct3D 12 backend, for the files that have to name a device.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include, and EVERY CLIENT OF IT IS IN THIS FOLDER - which is the
// rule rather than a count. cmake/check_engine_includes.cmake fails the build
// for any file outside engine/render/<backend>/ that names any header inside
// it: the folder, not one filename in it. Stated as the rule and not as a
// list of names, because a list goes stale. device_resources.cpp is not among
// them and structurally could not be - it is what this header includes, not
// something that includes it.
//
// WHAT THIS BACKEND DOES DIFFERENTLY, AND WHY IT WAS WORTH WRITING AT ALL. It
// reaches less hardware than the D3D11 backend, not more: feature level 11_0
// and Windows 10 against that one's 10.0 (device_resources.cpp says so where
// the floor is set). It is here for two things none of the other backends
// could give when it was written - there were three then and there are four
// now, the fifth being the Vulkan one, which answers the first of the two the
// same way and is the second answer to it rather than a repeat.
//
//  - THIS IS THE ONE API BEHIND THE SEAM WHERE THE ENGINE OWNS
//    SYNCHRONISATION.
//    D3D11 renames a mapped buffer for you and tracks what is still in flight;
//    OpenGL's driver does the same behind glBufferSubData; the null backend has
//    no GPU to be out of step with. Here the engine holds the fence: frames in
//    flight are a number this file picks (DeviceResources::FRAME_COUNT), a
//    command allocator may not be reset until the GPU has finished reading it,
//    a vertex page written this frame may still be being read next frame, and a
//    texture upload is a copy the CPU must wait for. Every one of those is
//    below the seam and none of them reached a line of it. That is the claim,
//    and this backend is the test of it.
//
//  - A SECOND RASTERISER CI CAN RUN. .github/workflows/ci.yml skips
//    RenderPixelTests on the OpenGL preset, because a runner has no GPU and
//    Windows' OpenGL fallback is GDI 1.1. Direct3D gets a device on that
//    machine, so this preset checks the pixel contract - against the same
//    tests/render/golden/ images the D3D11 preset is checked against, on
//    whatever adapter the runner offers both of them. That file says how the
//    adapter is known to be an adapter rather than the WARP fallback, and why
//    a log cannot say more than that.
//
// THE RECORDING SHAPE IS D3D11'S, NOT OPENGL'S. A view records into its own
// ID3D12GraphicsCommandList on its own worker thread, and submit() hands the
// finished lists to the queue in view order - which is what a deferred context
// does next door, spelt differently. The difference is what goes with a list:
// a command allocator PER FRAME IN FLIGHT rather than one, because the memory a
// list records into belongs to the allocator and cannot be reused until the GPU
// is done with it.
//
// The rest, each written down where it happens: the transform is four root
// constants rather than a constant buffer, so there is no per-view buffer to
// map or to rebuild (renderer.cpp, create_device_dependent_resources);
// the vertex ring is per frame in flight and grows in pages rather than
// wrapping one buffer with DISCARD (DrawList::View::VertexPage); every texture
// is a copy through an upload buffer the load path waits for
// (texture_factory.cpp); the descriptor heap textures live in is fixed size and
// says so by name when it is full (Renderer::Impl::allocate_texture_slot); and
// the seam's three debug markers do nothing, because the event encoding a
// command queue takes is WinPixEventRuntime's and this repository does not buy
// libraries it can do without (T9).

namespace labrador
{
	// A texture, owned: the resource, where its descriptor is, and how big it
	// is.
	//
	// THE DESCRIPTOR IS A SLOT NUMBER AND NOT A HANDLE, which matters more than
	// it looks. A GPU descriptor handle is a pointer into one particular heap,
	// so storing one here would mean every texture in the table went stale the
	// moment the heap was remade - which a device loss does. A slot is an index
	// into whatever heap the renderer currently has, resolved at the draw, and
	// it survives exactly as the engine's own handles do (registry.h has the
	// same argument about pointers and slots).
	//
	// The size rides along because engine/render/sprite_geometry.h needs it
	// every draw and asking a D3D12 resource for its description is a copy of a
	// struct rather than the three virtual calls the D3D11 backend makes - a
	// GetResource, the QueryInterface inside its As(), and a GetDesc.
	class D3d12Texture
	{
	public:
		D3d12Texture(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
			int slot, int width, int height)
			: resource_(std::move(resource)), slot_(slot), width_(width),
			  height_(height) {}

		D3d12Texture(const D3d12Texture&) = delete;
		D3d12Texture& operator=(const D3d12Texture&) = delete;

		int slot() const { return this->slot_; }
		mattmath::Vector2F size() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
		int slot_ = 0;
		int width_ = 0;
		int height_ = 0;
	};

	// Where a named texture lives, and only a texture. This class held all
	// three tables once and the sentence here still said so after two of them
	// left: fonts and sheets are engine data and are members of RenderResources
	// itself now (engine/render/render_resources.h says why, and what it
	// saved), so what is behind the pimpl is the one table whose resource type
	// this folder owns.
	class RenderResources::Impl
	{
	public:
		// One table, and it is the only one whose resource type this folder
		// owns. The font and sheet tables are RenderResources' own members;
		// engine/render/render_resources.h says why, and what it saved.
		void add_texture(const std::string& name,
			std::unique_ptr<D3d12Texture> texture);

		void release_all_textures();

		// The descriptor slot the name already holds, or -1 if it holds none -
		// which is the answer for a name nobody has loaded and for one whose
		// texture a device loss released, because the heap those slots were in
		// has gone with it.
		int descriptor_slot(const std::string& name) const;

		const D3d12Texture* texture(TextureHandle texture) const;

		Registry<D3d12Texture> textures{ "Texture" };
	};

	// One view's recording: a command list, an allocator per frame in flight,
	// and the vertex memory the list points at.
	//
	// DrawList holds a pointer to one of these and nothing else, which is what
	// makes a DrawList trivially copyable and free to pass.
	class DrawList::View
	{
	public:
		// How many sprites one vertex page holds. The same 2048 the D3D11
		// backend uses, arrived at the same way: 16-bit indices cap a page at
		// 16384 sprites, and 2048 of them is 256KB - affordable on the low tier
		// and large enough that the fill-and-flush path is not what a frame
		// spends its time on. The index buffer is built once for this many and
		// reused by every page.
		static const int MAX_PAGE_SPRITES = 2048;

		// One page of one view's vertex memory for one frame in flight.
		//
		// AN UPLOAD-HEAP BUFFER, MAPPED ONCE AND NEVER UNMAPPED. That is the
		// documented way to write vertices from the CPU in this API, and it is
		// what replaces D3D11's Map(DISCARD)/Map(NO_OVERWRITE) pair - there,
		// the driver hands back fresh memory when a buffer fills and tracks
		// what the GPU is still reading; here nobody does either, so the memory
		// has to be memory the GPU cannot still be reading. That is what the
		// per-frame array below is for: page N of frame 0 is written again only
		// after the fence says frame 0's last submission finished. That wait is
		// not in this file either - View::begin names the two places it happens
		// and why the resize path needs the stronger of them.
		//
		// PAGES GROW, THEY DO NOT WRAP. D3D11 wraps its one buffer at 2048
		// sprites and asks for a DISCARD; wrapping here would overwrite
		// vertices this frame's own already-recorded draw calls point at. So a
		// view that outgrows a page takes another one, and keeps it for the
		// life of the process - which is the OpenGL backend's answer (its
		// vertex store grows without a cap) reached from the opposite
		// direction.
		struct VertexPage
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

			// Into the mapped upload buffer. Written, never read: reading back
			// from upload memory is a documented way to make a frame slow.
			SpriteVertex* mapped = nullptr;

			D3D12_GPU_VIRTUAL_ADDRESS address = 0;
		};

		Renderer::Impl* owner = nullptr;

		// ONE LIST, AN ALLOCATOR EACH. The list is reset onto this frame's
		// allocator at the top of every frame; the allocator it was reset onto
		// last frame is still being read by the GPU, which is the whole reason
		// there is more than one.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
			allocators[DeviceResources::FRAME_COUNT];

		std::vector<VertexPage> pages[DeviceResources::FRAME_COUNT];

		// Where the next batch goes: which page, and how many sprites into it.
		int page = 0;
		int page_position = 0;

		// The sprites recorded since the last flush, four corners each. Built
		// on the CPU and copied in one go, because a copy per sprite into
		// mapped memory is a write-combining stall per sprite.
		std::vector<SpriteVertex> batch;

		// What `batch` is drawn with. A change is what a flush is for: one
		// texture and one sampler per draw call.
		const D3d12Texture* batch_texture = nullptr;

		// Pixels to clip space for the current viewport, set as root constants
		// on every flush. Held rather than recomputed because set_viewport is
		// what changes it and draws are what read it.
		float transform[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Reset at the top of every frame, not carried across one.
		Camera camera = Camera::DEFAULT_CAMERA;
		TextureFilter filter = TextureFilter::point;

		// Whether anything has been recorded into this view this frame. It is
		// what makes a dropped view detectable: a caller that declares four
		// views, fills them, and then declares two has stranded two recordings.
		// See Renderer::set_view_count.
		bool touched = false;

		// Whether this frame's allocator has been reset and this frame's
		// render target bound into the list - which is to say whether the list
		// is open for recording. Deferred to set_view_count for the reason the
		// D3D11 backend defers its binding: that is the first moment anybody
		// knows which views this frame has. What makes the allocator safe to
		// reset by then is a wait, and which wait it was depends on the caller;
		// View::begin names both.
		bool recording = false;

		// Appends one sprite's four corners, flushing first if it cannot join
		// what is already there - a different texture, or a full page.
		void draw(const D3d12Texture& texture, const SpriteVertex* corners);

		// Records one draw call for everything appended since the last flush,
		// and nothing at all when nothing has been.
		void flush();

		// Resets this frame's allocator, opens the list, and records the state
		// that does not change within a frame. Idempotent within a frame.
		void begin(D3D12_CPU_DESCRIPTOR_HANDLE render_target,
			const D3D12_VIEWPORT& viewport);

		// Pixels to clip space for this viewport, into `transform`.
		void set_transform(const D3D12_VIEWPORT& viewport);

		// Flushes what is batched and closes the list, which is what makes it
		// executable. Does nothing on a view that is not recording.
		void close();

		void reset();
	};

	// The device, the swap chain, the views, and everything a draw call needs
	// that is the same for every draw call.
	//
	// It implements D3DDeviceNotify itself and forwards to the seam's
	// DeviceNotify, so the game hears about a device loss without ever hearing
	// what a device is.
	class Renderer::Impl final : public D3DDeviceNotify
	{
	public:
		Impl();
		~Impl();

		// HOW MANY DISTINCT TEXTURE NAMES MAY BE LIVE AT ONCE, and it is a
		// fixed number for a reason this API imposes: the heap a shader samples
		// from is created at one size, and growing one means either a second
		// non-shader-visible heap to copy descriptors out of or rebuilding
		// every view that names them. Neither is machinery this engine's
		// content needs - both clients together load a few dozen textures - so
		// the limit is stated, and allocate_texture_slot refuses by name rather
		// than overrunning (T6: say what went wrong, do not carry on).
		//
		// NAMES AND NOT LOADS, which is the term to read carefully because
		// nothing here gives a slot back one at a time. A name keeps the slot
		// it was given for as long as the heap lives, so re-loading a texture
		// under a name that has one writes into that slot rather than taking
		// another (texture_factory.cpp), and a client that walks its manifest
		// once per level costs the heap nothing after the first walk. The only
		// thing that returns slots is a device loss, which remakes the heap
		// whole and resets the allocator to zero.
		static const int TEXTURE_CAPACITY = 256;

		DeviceResources device_resources{ DXGI_FORMAT_B8G8R8A8_UNORM };

		std::vector<std::unique_ptr<DrawList::View>> views;
		int view_count = 0;

		// BETWEEN begin_frame AND end_frame, WHICH IS A WIDER INTERVAL THAN
		// "SOMETHING IS RECORDING" - and it is the wider one renderer.h
		// legislates. Nothing of this frame is open between begin_frame and the
		// first set_view_count: open_frame executes the frame list on its way
		// out and opens no views while the count is still zero. A resize
		// arriving in that window still has a frame to restart, and asking the
		// command lists whether there is one answers no - which costs that
		// frame its clear and its PRESENT -> RENDER_TARGET barrier. So the
		// frame is tracked rather than inferred, and this is the whole of the
		// tracking.
		bool frame_begun = false;

		// Whether submit() has already run for this frame. renderer.h makes a
		// second submit a no-op; this is the whole of what that costs.
		bool frame_submitted = false;

		// SHARED BY EVERY VIEW, because nothing writes to any of them once they
		// are made. The index buffer is the same two triangles per sprite for
		// every sprite there will ever be, uploaded once at device creation.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
		Microsoft::WRL::ComPtr<ID3D12Resource> indices;
		D3D12_INDEX_BUFFER_VIEW index_view = {};

		// Shader-visible, both of them, because a draw call names descriptors
		// in them by GPU handle. The sampler heap holds exactly two entries -
		// point and linear - which is the whole of what the seam's
		// TextureFilter can ask for.
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> texture_heap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_heap;
		UINT texture_descriptor_size = 0;
		UINT sampler_descriptor_size = 0;
		int next_texture_slot = 0;

		// THE FRAME'S OWN LIST, WHICH IS EVERYTHING THAT IS NOT A VIEW DRAWING:
		// the clear, the two back-buffer transitions, the read-back copy, every
		// texture upload, and the index buffer's own upload and barrier at
		// device creation - which is on the list for the same reason as the
		// rest and was the one item this census left out. A view's list may not carry them - a view is
		// recorded on a worker thread and there are none of it or four,
		// depending on the frame, so "the first one" is not a thing that
		// exists.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> frame_list;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
			frame_allocators[DeviceResources::FRAME_COUNT];
		bool frame_list_open = false;

		const RenderResources* resources = nullptr;
		labrador::DeviceNotify* notify = nullptr;

		D3D12_GPU_DESCRIPTOR_HANDLE sampler(TextureFilter filter) const;

		// Claims a descriptor slot for a texture. Throws std::runtime_error
		// naming the limit when there are none left.
		int allocate_texture_slot(const std::string& name);

		D3D12_CPU_DESCRIPTOR_HANDLE texture_slot_cpu(int slot) const;
		D3D12_GPU_DESCRIPTOR_HANDLE texture_slot_gpu(int slot) const;

		// Closes every open command list and forgets what was recorded into
		// it, without executing any of it. Two callers, and they want it for
		// the same reason: a frame nobody will submit (begin_frame) and a
		// frame whose back buffer is about to be destroyed
		// (window_size_changed).
		void abandon_recording();

		// Clears the current back buffer and opens the views the frame has
		// declared against it. The second half of begin_frame, and the whole
		// of what putting a resized frame back on its feet takes.
		void open_frame();

		// Opens the frame list for recording, resetting it onto this frame's
		// allocator if it is closed. Returns the same list either way, which is
		// what makes it idempotent WITHIN one operation: the barrier and the
		// clear of open_frame are two calls to this and one list, as are the
		// transition, the copy and the transition back of read_back_buffer. It
		// is not a way to add to what begin_frame recorded - every entry point
		// executes and closes the list before it returns, so there is never an
		// open one between them.
		ID3D12GraphicsCommandList* open_frame_list();

		// Closes and executes it, and signals the fence. Does nothing if
		// nothing opened it.
		void execute_frame_list();

		// Transitions the back buffer into `state`, recording the barrier into
		// the frame list and remembering what it did. Does nothing when it is
		// already in that state.
		void transition_back_buffer(D3D12_RESOURCE_STATES state);

		// Everything above that belongs to the device, remade with it.
		void create_device_dependent_resources();

		void on_device_lost() override;
		void on_device_restored() override;
	};
}

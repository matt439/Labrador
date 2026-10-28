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
// Engine includers stay in this folder; backend-specific tests may include it
// deliberately to inspect resources. The check in
// cmake/check_engine_includes.cmake scans engine sources and refuses any other
// engine folder naming a header inside this one. The wall covers the folder,
// not a list of filenames, so a new backend header is covered automatically.
//
// Each view records its own ID3D12GraphicsCommandList; submit() queues the
// finished lists in view order. Each view has an allocator and vertex pages
// per frame slot, reusable only after that slot's fence target is reached.
// DeviceResources owns these synchronization boundaries below the neutral seam
// in engine/render/renderer.h.
//
// View transforms use root constants, textures upload through a waited staging
// buffer, and shader-visible texture descriptors occupy a fixed-capacity heap.
// Debug markers are no-ops: their queue encoding would require the otherwise
// unused WinPixEventRuntime dependency (T9). Device and pixel-test evidence is
// recorded in docs/review/d3d12/STATUS.md and docs/review/INDEX.md.

namespace labrador
{
	// Owns a texture resource, its dimensions and an index into the renderer's
	// texture descriptor heap. Draws resolve that index against the current
	// heap. Device loss releases textures and recreates the heap; the resource
	// loader repopulates both. Geometry reads cached dimensions without querying
	// the API resource on each draw (engine/render/sprite_geometry.h).
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
		Vector2F size() const;

		// Borrowed for backend operations; callers preserve the shader-resource
		// state and finish queued work before this texture is released.
		ID3D12Resource* resource() const { return this->resource_.Get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
		int slot_ = 0;
		int width_ = 0;
		int height_ = 0;
	};

	// Owns the backend texture table. Font and sheet tables belong to the
	// shared RenderResources in engine/render/render_resources.h.
	class RenderResources::Impl
	{
	public:
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
		// Sprites per vertex page, within the 16-bit index limit of 16384.
		// The shared index buffer covers one page and is reused by every page.
		static const int max_page_sprites = 2048;

		// One page of one view's vertex memory for one frame in flight.
		//
		// Persistently mapped upload memory, written again only after the slot's
		// fence target is reached. begin_frame waits for reuse; resize drains
		// queued work before rebuilding the back buffers and reopening views.
		// A full page grows the vector instead of wrapping onto vertices that
		// recorded draws still reference. Pages remain until device recreation
		// or renderer destruction.
		struct VertexPage
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

			// Into the mapped upload buffer. Written, never read: reading back
			// from upload memory is a documented way to make a frame slow.
			SpriteVertex* mapped = nullptr;

			D3D12_GPU_VIRTUAL_ADDRESS address = 0;
		};

		Renderer::Impl* owner = nullptr;

		// The list is reset onto the current slot's allocator after its fence
		// wait. Other slots' allocator memory may still be in use by the GPU.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
			allocators[DeviceResources::frame_count];

		std::vector<VertexPage> pages[DeviceResources::frame_count];

		// Where the next batch goes: which page, and how many sprites into it.
		int page = 0;
		int page_position = 0;

		// Sprites since the last flush, four corners each. Built in CPU memory
		// and copied together into the mapped upload page.
		std::vector<SpriteVertex> batch;

		// What `batch` is drawn with. A change is what a flush is for: one
		// texture and one sampler per draw call.
		const D3d12Texture* batch_texture = nullptr;

		// Pixels to clip space for the current viewport, set as root constants
		// on every flush. Held rather than recomputed because set_viewport is
		// what changes it and draws are what read it.
		float transform[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Reset at the top of every frame, not carried across one.
		Camera camera = Camera::default_camera;
		TextureFilter filter = TextureFilter::point;

		// The pane DrawList::viewport() answers with: the Viewport last given
		// to set_viewport, as given, else the screen viewport the view was
		// bound with. Only read back - what the rasteriser uses is the D3D
		// viewport recorded into the list beside it.
		Viewport viewport;

		// Whether anything has been recorded into this view this frame. It is
		// what makes a dropped view detectable: a caller that declares four
		// views, fills them, and then declares two has stranded two recordings.
		// See Renderer::set_view_count.
		bool touched = false;

		// Whether the current slot's allocator has been reset and its render
		// target bound for recording. set_view_count opens declared views after
		// begin_frame's slot wait; resize reopens them after draining the GPU.
		bool recording = false;

		// Appends one sprite's four corners, flushing first if it cannot join
		// what is already there - a different texture, or a full page.
		void draw(const D3d12Texture& texture, const SpriteVertex* corners);

		// Records one draw call for everything appended since the last flush,
		// and nothing at all when nothing has been.
		void flush();

		// Resets this slot's allocator, opens the list and binds drawing state.
		// The caller must have waited for queued uses of the slot to finish.
		// Does nothing if the view is already recording.
		void begin(D3D12_CPU_DESCRIPTOR_HANDLE render_target,
			const D3D12_VIEWPORT& pane);

		// Pixels to clip space for this viewport, into `transform`.
		void set_transform(const D3D12_VIEWPORT& pane);

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

		// Distinct texture names supported by the fixed shader-visible heap.
		// Reloading a name reuses its slot; individual slots are not returned.
		// Device recreation resets allocation with the heap. Exhaustion throws
		// through allocate_texture_slot instead of overrunning (T6).
		static const int texture_capacity = 256;

		DeviceResources device_resources{ DXGI_FORMAT_B8G8R8A8_UNORM };
		RenderDeviceInfo device_info;

		std::vector<std::unique_ptr<DrawList::View>> views;
		// Non-owning scratch, reserved with the view capacity. A successful
		// submission clears it after the queue has consumed the pointer array.
		std::vector<ID3D12CommandList*> submission_lists;
		int view_count = 0;

		// Between begin_frame and end_frame, including before any view opens.
		// A resize in that interval must restore the frame's clear and barrier
		// even when no command list is currently recording.
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

		// Renderer-owned commands: clear, back-buffer transitions, read-back,
		// texture uploads and index upload/barrier during device creation.
		// View lists may record concurrently and there may be no declared views,
		// so commands shared by the frame cannot belong to an arbitrary view.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> frame_list;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
			frame_allocators[DeviceResources::frame_count];
		bool frame_list_open = false;

		const RenderResources* resources = nullptr;
		labrador::DeviceNotify* notify = nullptr;

		D3D12_GPU_DESCRIPTOR_HANDLE sampler(TextureFilter filter) const;

		// Claims a descriptor slot for a texture. Throws std::runtime_error
		// naming the limit when there are none left.
		int allocate_texture_slot(const std::string& name);

		D3D12_CPU_DESCRIPTOR_HANDLE texture_slot_cpu(int slot) const;
		D3D12_GPU_DESCRIPTOR_HANDLE texture_slot_gpu(int slot) const;

		// Closes open lists without executing them. Used before beginning a new
		// frame or destroying a back buffer during resize.
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

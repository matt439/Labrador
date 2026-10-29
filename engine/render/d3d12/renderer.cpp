#include "engine/render/d3d12/backend.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/text_encoding.h"
#include "engine/render/throw_if_failed.h"

#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace labrador;
using Microsoft::WRL::ComPtr;

namespace labrador
{
	namespace
	{
		RenderDeviceInfo selected_device(ID3D12Device* device)
		{
			ComPtr<IDXGIFactory4> factory;
			ThrowIfFailed(CreateDXGIFactory1(
				IID_PPV_ARGS(factory.GetAddressOf())));

			ComPtr<IDXGIAdapter1> adapter;
			const LUID selected = device->GetAdapterLuid();
			const HRESULT enumeration = factory->EnumAdapterByLuid(selected,
				IID_PPV_ARGS(adapter.GetAddressOf()));
			if (FAILED(enumeration))
			{
				// WARP is obtained through EnumWarpAdapter rather than the
				// ordinary adapter walk. Some DXGI versions therefore do not
				// return it from EnumAdapterByLuid even though the device exposes
				// its LUID. Ask for that one explicitly, then prove it is the
				// device's rather than silently reporting a fallback that was not
				// selected.
				ComPtr<IDXGIAdapter> warp;
				ThrowIfFailed(factory->EnumWarpAdapter(
					IID_PPV_ARGS(warp.GetAddressOf())));
				ThrowIfFailed(warp.As(&adapter));

				DXGI_ADAPTER_DESC1 warp_description = {};
				ThrowIfFailed(adapter->GetDesc1(&warp_description));
				if (warp_description.AdapterLuid.HighPart != selected.HighPart ||
					warp_description.AdapterLuid.LowPart != selected.LowPart)
				{
					ThrowIfFailed(enumeration);
				}
			}

			DXGI_ADAPTER_DESC1 description = {};
			ThrowIfFailed(adapter->GetDesc1(&description));

			RenderDeviceInfo info;
			info.backend = "d3d12";
			info.present_mode = "dxgi_sync_interval";
			info.requested_swap_interval = 1;
			info.api = "Direct3D 12";
			info.device_name = narrow(description.Description);
			info.vendor_id = description.VendorId;
			info.device_id = description.DeviceId;
			info.kind = (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0
				? RenderDeviceKind::software : RenderDeviceKind::hardware;
			return info;
		}

		// Four corners, two triangles, and the winding the corner order in
		// sprite_geometry.h fixes.
		const int vertices_per_sprite = 4;
		const int indices_per_sprite = 6;

		// Where the engine's values become this API's.
		//
		// D3D12_VIEWPORT is D3D11_VIEWPORT with different spelling, and the
		// truncation is the same deliberate one: the extents are FLOAT and a
		// fractional viewport is legal, but viewport.h says pixel_rect is the
		// only conversion to whole pixels any backend may make, so every
		// backend makes the same one.
		D3D12_VIEWPORT to_d3d_viewport(const Viewport& viewport)
		{
			const RectangleI pixels = viewport.pixel_rect();

			return { static_cast<float>(pixels.x),
				static_cast<float>(pixels.y),
				static_cast<float>(pixels.width),
				static_cast<float>(pixels.height),
				viewport.minDepth, viewport.maxDepth };
		}

		// THE SCISSOR IS NOT OPTIONAL, and it is the one line of this file a
		// reader coming from the D3D11 backend will not expect. There, scissor
		// testing is off in the rasteriser state and the viewport alone decides
		// what is drawn. Here there is no such state: a command list starts with
		// an empty scissor rectangle, so a list that sets a viewport and no
		// scissor rasterises nothing at all and reports nothing wrong. Every
		// place that sets one sets the other, from the same rectangle.
		D3D12_RECT to_scissor(const D3D12_VIEWPORT& viewport)
		{
			return { static_cast<LONG>(viewport.TopLeftX),
				static_cast<LONG>(viewport.TopLeftY),
				static_cast<LONG>(viewport.TopLeftX + viewport.Width),
				static_cast<LONG>(viewport.TopLeftY + viewport.Height) };
		}

		D3D12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE type)
		{
			D3D12_HEAP_PROPERTIES properties = {};
			properties.Type = type;
			properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			properties.CreationNodeMask = 1;
			properties.VisibleNodeMask = 1;
			return properties;
		}

		D3D12_RESOURCE_DESC buffer_description(UINT64 bytes)
		{
			D3D12_RESOURCE_DESC description = {};
			description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			description.Alignment = 0;
			description.Width = bytes;
			description.Height = 1;
			description.DepthOrArraySize = 1;
			description.MipLevels = 1;
			description.Format = DXGI_FORMAT_UNKNOWN;
			description.SampleDesc.Count = 1;
			description.SampleDesc.Quality = 0;
			description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			description.Flags = D3D12_RESOURCE_FLAG_NONE;
			return description;
		}

		ComPtr<ID3D12Resource> create_buffer(ID3D12Device* device,
			D3D12_HEAP_TYPE heap, UINT64 bytes, D3D12_RESOURCE_STATES state)
		{
			const D3D12_HEAP_PROPERTIES properties = heap_properties(heap);
			const D3D12_RESOURCE_DESC description = buffer_description(bytes);

			ComPtr<ID3D12Resource> buffer;
			ThrowIfFailed(device->CreateCommittedResource(&properties,
				D3D12_HEAP_FLAG_NONE, &description, state, nullptr,
				IID_PPV_ARGS(buffer.GetAddressOf())));
			return buffer;
		}

		// Pixels to clip space.
		//
		// THE ONE LINE WHERE Y RUNNING DOWN IS DECIDED, for this backend, and
		// it is the same four numbers the D3D11 one writes because the two
		// share a clip space: y increases up, x and y run -1 to 1, and the seam
		// (sprite_vertex.h) hands over pixels with y increasing down. The GL
		// backend writes them differently and says so. The viewport's position
		// is not in them: the rasteriser already maps clip space onto the
		// viewport rectangle.
		void fill_transform(float* transform, const D3D12_VIEWPORT& viewport)
		{
			transform[0] = viewport.Width > 0.0f ? 2.0f / viewport.Width : 0.0f;
			transform[1] =
				viewport.Height > 0.0f ? -2.0f / viewport.Height : 0.0f;
			transform[2] = -1.0f;
			transform[3] = 1.0f;
		}
	}

	// --- D3d12Texture --------------------------------------------------------

	Vector2F D3d12Texture::size() const
	{
		return Vector2F(static_cast<float>(this->width_),
			static_cast<float>(this->height_));
	}

	// --- DrawList::View ------------------------------------------------------

	// Nothing is recorded until a flush, so a view the scene declared and then
	// drew nothing into records nothing at all.
	void DrawList::View::draw(const D3d12Texture& texture,
		const SpriteVertex* corners)
	{
		// ONE TEXTURE PER DRAW CALL, so a run of sprites sharing one is one
		// call and a change is a flush. That is the whole of the batching, and
		// it is the same on every backend that draws.
		if (&texture != this->batch_texture ||
			this->batch.size() >= static_cast<size_t>(max_page_sprites) *
				vertices_per_sprite)
		{
			this->flush();
			this->batch_texture = &texture;
		}

		this->batch.insert(this->batch.end(), corners,
			corners + vertices_per_sprite);
		this->touched = true;
	}

	void DrawList::View::flush()
	{
		if (this->batch.empty())
		{
			return;
		}

		const int sprites =
			static_cast<int>(this->batch.size()) / vertices_per_sprite;

		const int frame = this->owner->device_resources.frame_index();
		std::vector<VertexPage>& frame_pages =
			this->pages[static_cast<size_t>(frame)];

		// A NEW PAGE RATHER THAN A WRAP, which is the whole difference from the
		// D3D11 backend's flush. There, a full buffer is remapped with DISCARD
		// and the driver hands back fresh memory while the GPU finishes reading
		// the old; here nobody is tracking that, and the draw calls already
		// recorded into this list point into this page. So the page is left
		// alone and the next one is taken.
		if (this->page_position + sprites > max_page_sprites)
		{
			this->page++;
			this->page_position = 0;
		}

		while (frame_pages.size() <= static_cast<size_t>(this->page))
		{
			VertexPage fresh;
			fresh.buffer = create_buffer(this->owner->device_resources.device(),
				D3D12_HEAP_TYPE_UPLOAD,
				static_cast<UINT64>(max_page_sprites) * vertices_per_sprite *
					sizeof(SpriteVertex),
				D3D12_RESOURCE_STATE_GENERIC_READ);

			// Mapped once and never unmapped, which is what this heap type is
			// for. The read range is empty because nothing here ever reads it.
			void* mapped = nullptr;
			const D3D12_RANGE nothing_read = { 0, 0 };
			ThrowIfFailed(fresh.buffer->Map(0, &nothing_read, &mapped));

			fresh.mapped = static_cast<SpriteVertex*>(mapped);
			fresh.address = fresh.buffer->GetGPUVirtualAddress();
			frame_pages.push_back(std::move(fresh));
		}

		const VertexPage& page_in_use =
			frame_pages[static_cast<size_t>(this->page)];

		std::memcpy(page_in_use.mapped +
				static_cast<size_t>(this->page_position) * vertices_per_sprite,
			this->batch.data(),
			this->batch.size() * sizeof(SpriteVertex));

		Renderer::Impl& owner_impl = *this->owner;

		// WHAT IS SET HERE AND WHAT IS SET IN begin(), which is a line the
		// D3D11 backend does not have to draw. There, everything is set on
		// every flush - a dozen more commands into a deferred context, and no
		// rule to remember about what survives a viewport change. Here two of
		// those commands are not free: SetDescriptorHeaps is documented as
		// expensive and can cost a pipeline flush on some drivers, and the root
		// signature invalidates every root binding when it is set. Both are
		// invariant for the life of a list, so both are in begin(); everything
		// that varies per draw call is here.
		D3D12_VERTEX_BUFFER_VIEW vertices = {};
		vertices.BufferLocation = page_in_use.address;
		vertices.SizeInBytes = static_cast<UINT>(
			static_cast<size_t>(max_page_sprites) * vertices_per_sprite *
			sizeof(SpriteVertex));
		vertices.StrideInBytes = static_cast<UINT>(sizeof(SpriteVertex));

		this->list->IASetVertexBuffers(0, 1, &vertices);
		this->list->SetGraphicsRoot32BitConstants(0, 4, this->transform, 0);
		this->list->SetGraphicsRootDescriptorTable(1,
			owner_impl.texture_slot_gpu(this->batch_texture->slot()));
		this->list->SetGraphicsRootDescriptorTable(2,
			owner_impl.sampler(this->filter));

		this->list->DrawIndexedInstanced(
			static_cast<UINT>(sprites * indices_per_sprite), 1,
			static_cast<UINT>(this->page_position * indices_per_sprite), 0, 0);

		this->page_position += sprites;
		this->batch.clear();
	}

	void DrawList::View::set_transform(const D3D12_VIEWPORT& pane)
	{
		fill_transform(this->transform, pane);
	}

	void DrawList::View::begin(D3D12_CPU_DESCRIPTOR_HANDLE render_target,
		const D3D12_VIEWPORT& pane)
	{
		if (this->recording)
		{
			return;
		}

		Renderer::Impl& owner_impl = *this->owner;
		const int frame = owner_impl.device_resources.frame_index();

		// THE ALLOCATOR IS RESET HERE AND NOWHERE ELSE, AND SOMETHING HAS
		// ALREADY WAITED FOR THE GPU TO FINISH WITH IT. That sentence is the
		// whole of what this API asks of the engine that D3D11, GL and null do
		// not: the memory a command list records into belongs to the allocator,
		// and reusing it while the GPU is still reading last time's commands is
		// not an error anything reports - it is a frame drawn from two frames'
		// commands at once. Vulkan asks exactly the same thing about a command
		// pool and opens its begin_frame with the same paragraph, which is what
		// makes this an API property rather than this backend's habit.
		//
		// WHICH WAIT IT WAS DEPENDS ON WHO CALLED, and there are two callers.
		// Renderer::begin_frame waits on the fence for this frame index before
		// anything else it does. Renderer::window_size_changed reaches this
		// through open_frame(), where the wait is the one inside
		// DeviceResources::create_window_size_dependent_resources - a full
		// wait_for_gpu, which is stronger, and which has to be: frame_index_ is
		// re-read from the swap chain after ResizeBuffers and may not be the
		// index begin_frame waited on.
		ThrowIfFailed(this->allocators[static_cast<size_t>(frame)]->Reset());
		ThrowIfFailed(this->list->Reset(
			this->allocators[static_cast<size_t>(frame)].Get(),
			owner_impl.sprite_pipeline.pipeline_state()));

		ID3D12DescriptorHeap* heaps[] = { owner_impl.texture_heap.Get(),
			owner_impl.sampler_heap.Get() };
		this->list->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)),
			heaps);
		this->list->SetGraphicsRootSignature(
			owner_impl.sprite_pipeline.root_signature());

		this->list->IASetPrimitiveTopology(
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		this->list->IASetIndexBuffer(&owner_impl.index_view);

		// FALSE: the descriptors are not contiguous, there being one of them.
		this->list->OMSetRenderTargets(1, &render_target, FALSE, nullptr);

		const D3D12_RECT scissor = to_scissor(pane);
		this->list->RSSetViewports(1, &pane);
		this->list->RSSetScissorRects(1, &scissor);
		this->set_transform(pane);
		this->viewport = Viewport(pane.TopLeftX, pane.TopLeftY, pane.Width,
			pane.Height, pane.MinDepth, pane.MaxDepth);

		this->page = 0;
		this->page_position = 0;
		this->recording = true;
	}

	void DrawList::View::close()
	{
		if (!this->recording)
		{
			return;
		}

		this->flush();
		ThrowIfFailed(this->list->Close());
		this->recording = false;
	}

	void DrawList::View::reset()
	{
		this->camera = Camera::default_camera;
		this->filter = TextureFilter::point;
		this->batch.clear();
		this->batch_texture = nullptr;
		this->page = 0;
		this->page_position = 0;
		this->touched = false;
	}

	// --- DrawList ------------------------------------------------------------

	void DrawList::set_viewport(const Viewport& viewport)
	{
		// Everything already recorded belongs to the old viewport, so it goes
		// out first. Both the rasteriser's rectangle and the transform the
		// vertex shader projects with follow from the same one.
		this->view_->flush();

		const D3D12_VIEWPORT d3d_viewport = to_d3d_viewport(viewport);
		const D3D12_RECT scissor = to_scissor(d3d_viewport);
		this->view_->list->RSSetViewports(1, &d3d_viewport);
		this->view_->list->RSSetScissorRects(1, &scissor);
		this->view_->set_transform(d3d_viewport);
		this->view_->viewport = viewport;
	}

	void DrawList::set_camera(const Camera& camera)
	{
		// No flush: the camera is applied to the geometry as each draw is
		// recorded, not by any state the batch holds.
		this->view_->camera = camera;
	}

	const Camera& DrawList::camera() const
	{
		return this->view_->camera;
	}

	const Viewport& DrawList::viewport() const
	{
		return this->view_->viewport;
	}

	void DrawList::set_filter(TextureFilter filter)
	{
		if (this->view_->filter == filter)
		{
			return;
		}
		// One sampler per draw call, so a change is a flush - which is why the
		// seam says to group by filter if it matters.
		this->view_->flush();
		this->view_->filter = filter;
	}

	void DrawList::draw_sprite(TextureHandle texture,
		const RectangleI& source,
		const RectangleF& destination,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth)
	{
		// Orders nothing; engine/render/SEAM.md#6 says why it is a parameter.
		std::ignore = layer_depth;

		const D3d12Texture& device_texture =
			*this->view_->owner->resources->impl()->texture(texture);

		const ResolvedTexture<const D3d12Texture&> resolved{
			device_texture, device_texture.size() };
		record_sprite(*this->view_, resolved, source, destination, tint,
			rotation, origin, flip);
	}

	void DrawList::draw_text(FontHandle font,
		const std::wstring& text,
		const Vector2F& position,
		const Colour& tint,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth)
	{
		std::ignore = layer_depth;

		// The font is the seam's table and its atlas this folder's
		// (engine/render/render_resources.h).
		const RenderResources& resources = *this->view_->owner->resources;
		const Font& the_font = *resources.font(font);
		const D3d12Texture& atlas =
			*resources.impl()->texture(the_font.atlas());

		const ResolvedTexture<const D3d12Texture&> resolved{
			atlas, atlas.size() };
		record_text(*this->view_, the_font, resolved, text, position, tint,
			scale, rotation, origin);
	}

	// --- Renderer::Impl ------------------------------------------------------

	Renderer::Impl::Impl()
	{
		this->device_resources.register_device_notify(this);
	}

	Renderer::Impl::~Impl()
	{
		// BEFORE ANY OF THE MEMBERS BELOW device_resources GO, which is why
		// this destructor exists at all. Members are destroyed in reverse
		// declaration order, so device_resources - declared first - is
		// destroyed last, and the command lists, allocators and vertex pages
		// declared after it would be released while the GPU was still reading
		// them. The D3D11 backend has no such destructor because its runtime
		// keeps what is in flight alive; here that is this engine's job, which
		// is the point of the whole backend.
		//
		// AND IT ANSWERS RATHER THAN THROWS, for the reason ~DeviceResources
		// gives at the top of device_resources.cpp: this destructor is
		// implicitly noexcept, so a com_exception out of the wait is
		// std::terminate on an ordinary exit and the wait does not happen
		// either way. It is also the destructor a move-assignment runs, and
		// renderer.h declares Renderer's move operations noexcept. T6:
		// teardown stays silent.
		std::ignore = this->device_resources.try_wait_for_gpu();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::sampler(
		TextureFilter filter) const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle =
			this->sampler_heap->GetGPUDescriptorHandleForHeapStart();

		if (filter == TextureFilter::linear)
		{
			handle.ptr += this->sampler_descriptor_size;
		}
		return handle;
	}

	int Renderer::Impl::allocate_texture_slot(const std::string& name)
	{
		if (this->next_texture_slot >= texture_capacity)
		{
			throw std::runtime_error("Texture '" + name + "' does not fit: "
				"this backend's descriptor heap holds " +
				std::to_string(texture_capacity) + " distinct texture names "
				"and they are all taken. Re-loading a name already in it is "
				"free. See Renderer::Impl::texture_capacity.");
		}

		return this->next_texture_slot++;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::texture_slot_cpu(
		int slot) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			this->texture_heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(slot) * this->texture_descriptor_size;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::texture_slot_gpu(
		int slot) const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle =
			this->texture_heap->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<UINT64>(slot) * this->texture_descriptor_size;
		return handle;
	}

	void Renderer::Impl::abandon_recording()
	{
		// CLOSED, NOT EXECUTED, AND THAT IS THE WHOLE OPERATION. A command
		// list that is still recording cannot be reset, and neither can the
		// allocator underneath it, so a list nobody will execute still has to
		// be closed before its memory can be reused. What it holds goes
		// nowhere: on the begin_frame path it belongs to a frame nobody
		// submitted, and on the resize path it names a back buffer that is
		// about to stop existing.
		for (std::unique_ptr<DrawList::View>& view : this->views)
		{
			if (view->recording)
			{
				std::ignore = view->list->Close();
				view->recording = false;
			}
			view->reset();
		}

		if (this->frame_list_open)
		{
			std::ignore = this->frame_list->Close();
			this->frame_list_open = false;
		}
	}

	void Renderer::Impl::open_frame()
	{
		const int frame = this->device_resources.frame_index();
		ThrowIfFailed(
			this->frame_allocators[static_cast<size_t>(frame)]->Reset());

		this->transition_back_buffer(D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Opaque black, spelt out - the clear colour is a term
		// RenderPixelTests pins, so it is worth being able to read it here.
		const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		const D3D12_CPU_DESCRIPTOR_HANDLE render_target =
			this->device_resources.back_buffer_view();
		this->open_frame_list()->ClearRenderTargetView(render_target, black, 0,
			nullptr);
		this->execute_frame_list();

		// Nothing while the frame has not said how many views it has yet -
		// begin_frame, and a resize that beat the first set_view_count to it.
		// Everything on a resize that arrived after one.
		const D3D12_VIEWPORT viewport =
			this->device_resources.screen_viewport();
		for (int i = 0; i < this->view_count; i++)
		{
			this->views[static_cast<size_t>(i)]->begin(render_target,
				viewport);
		}
	}

	ID3D12GraphicsCommandList* Renderer::Impl::open_frame_list()
	{
		if (!this->frame_list_open)
		{
			const int frame = this->device_resources.frame_index();
			ThrowIfFailed(this->frame_list->Reset(
				this->frame_allocators[static_cast<size_t>(frame)].Get(),
				nullptr));
			this->frame_list_open = true;
		}

		return this->frame_list.Get();
	}

	void Renderer::Impl::execute_frame_list()
	{
		if (!this->frame_list_open)
		{
			return;
		}

		ThrowIfFailed(this->frame_list->Close());
		this->frame_list_open = false;

		ID3D12CommandList* lists[] = { this->frame_list.Get() };
		this->device_resources.command_queue()->ExecuteCommandLists(
			static_cast<UINT>(std::size(lists)), lists);
		this->device_resources.signal_frame();
	}

	void Renderer::Impl::transition_back_buffer(D3D12_RESOURCE_STATES state)
	{
		const D3D12_RESOURCE_STATES current =
			this->device_resources.back_buffer_state();
		if (current == state)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = this->device_resources.back_buffer();
		barrier.Transition.StateBefore = current;
		barrier.Transition.StateAfter = state;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		this->open_frame_list()->ResourceBarrier(1, &barrier);
		this->device_resources.set_back_buffer_state(state);
	}

	void Renderer::Impl::create_device_dependent_resources()
	{
		ID3D12Device* device = this->device_resources.device();

		this->sprite_pipeline.create(device,
			this->device_resources.back_buffer_format());

		// --- The descriptor heaps -------------------------------------------

		D3D12_DESCRIPTOR_HEAP_DESC texture_heap_description = {};
		texture_heap_description.Type =
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		texture_heap_description.NumDescriptors = texture_capacity;
		texture_heap_description.Flags =
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&texture_heap_description,
			IID_PPV_ARGS(this->texture_heap.ReleaseAndGetAddressOf())));

		D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_description = {};
		sampler_heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		sampler_heap_description.NumDescriptors = 2;
		sampler_heap_description.Flags =
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&sampler_heap_description,
			IID_PPV_ARGS(this->sampler_heap.ReleaseAndGetAddressOf())));

		this->texture_descriptor_size =
			device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		this->sampler_descriptor_size =
			device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		// The heap is new, so every slot in it is free - and the table that
		// named them has been emptied by the client's device-lost handler
		// (render_resources.h, release_device_resources), which reloads and
		// takes them again from zero.
		this->next_texture_slot = 0;

		// Clamped, so a source rectangle at the edge of an atlas cannot bleed
		// the far side of it into a sprite - which is what a sheet is full of.
		//
		// LEVEL ZERO, ALWAYS, and MaxLOD is where this backend says it: a
		// minified draw samples level zero however many levels the texture
		// carries, under either filter. renderer.h decides that beside
		// set_filter and says why it is the seam's decision and not a
		// backend's.
		D3D12_SAMPLER_DESC sampler_description = {};
		sampler_description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler_description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler_description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler_description.MipLODBias = 0.0f;
		sampler_description.MaxAnisotropy = 1;
		sampler_description.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler_description.MinLOD = 0.0f;
		sampler_description.MaxLOD = 0.0f;

		D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle =
			this->sampler_heap->GetCPUDescriptorHandleForHeapStart();

		sampler_description.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		device->CreateSampler(&sampler_description, sampler_handle);

		sampler_handle.ptr += this->sampler_descriptor_size;
		sampler_description.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		device->CreateSampler(&sampler_description, sampler_handle);

		// --- The lists, the allocators and the views ------------------------
		//
		// The frame's own list first, because the index buffer below is
		// uploaded through it.
		for (int i = 0; i < DeviceResources::frame_count; i++)
		{
			ThrowIfFailed(device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(
					this->frame_allocators[i].ReleaseAndGetAddressOf())));
		}

		ThrowIfFailed(device->CreateCommandList(0,
			D3D12_COMMAND_LIST_TYPE_DIRECT, this->frame_allocators[0].Get(),
			nullptr, IID_PPV_ARGS(
				this->frame_list.ReleaseAndGetAddressOf())));
		// CreateCommandList hands back an OPEN list, which is the one place
		// this API differs from every other create call in this file. Closed
		// here so that open_frame_list's Reset is the only thing that ever
		// opens it.
		ThrowIfFailed(this->frame_list->Close());
		this->frame_list_open = false;

		// A list and an allocator per frame in flight, per view - and the View
		// objects themselves are not remade, because a DrawList a caller is
		// holding must keep pointing at the same view.
		for (std::unique_ptr<DrawList::View>& view_ptr : this->views)
		{
			DrawList::View& view = *view_ptr;
			view.owner = this;

			for (int i = 0; i < DeviceResources::frame_count; i++)
			{
				ThrowIfFailed(device->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(
						view.allocators[i].ReleaseAndGetAddressOf())));

				// The pages go with the device that made them. They are not
				// recreated here: a page is taken the first time a frame needs
				// one, which is the same rule as on the first frame ever drawn.
				view.pages[i].clear();
			}

			ThrowIfFailed(device->CreateCommandList(0,
				D3D12_COMMAND_LIST_TYPE_DIRECT, view.allocators[0].Get(),
				this->sprite_pipeline.pipeline_state(),
				IID_PPV_ARGS(view.list.ReleaseAndGetAddressOf())));
			ThrowIfFailed(view.list->Close());
			view.recording = false;

			view.reset();
		}

		// --- The index buffer -----------------------------------------------
		//
		// The same two triangles per sprite for every sprite there will ever
		// be, so it is filled once and never touched again. IN A DEFAULT HEAP
		// AND COPIED THERE, where D3D11 says IMMUTABLE and hands over the bytes:
		// this API has no initial-data parameter, so "upload it once at
		// creation" is a staging buffer, a copy on a command list and a wait -
		// the same three steps every texture takes (texture_factory.cpp), which
		// is why the third file of this backend is the second longest of the
		// five. The longest is Vulkan's, for the same reason and one more: it
		// owns the allocation as well as the copy.
		std::vector<unsigned short> index_data;
		index_data.reserve(static_cast<size_t>(DrawList::View::
			max_page_sprites) * indices_per_sprite);
		for (int sprite = 0; sprite < DrawList::View::max_page_sprites;
			sprite++)
		{
			const unsigned short first =
				static_cast<unsigned short>(sprite * vertices_per_sprite);
			index_data.push_back(static_cast<unsigned short>(first + 0));
			index_data.push_back(static_cast<unsigned short>(first + 1));
			index_data.push_back(static_cast<unsigned short>(first + 2));
			index_data.push_back(static_cast<unsigned short>(first + 1));
			index_data.push_back(static_cast<unsigned short>(first + 3));
			index_data.push_back(static_cast<unsigned short>(first + 2));
		}

		const UINT64 index_bytes = static_cast<UINT64>(index_data.size()) *
			sizeof(unsigned short);

		this->indices = create_buffer(device, D3D12_HEAP_TYPE_DEFAULT,
			index_bytes, D3D12_RESOURCE_STATE_COMMON);

		ComPtr<ID3D12Resource> index_upload = create_buffer(device,
			D3D12_HEAP_TYPE_UPLOAD, index_bytes,
			D3D12_RESOURCE_STATE_GENERIC_READ);

		void* index_mapped = nullptr;
		const D3D12_RANGE nothing_read = { 0, 0 };
		ThrowIfFailed(index_upload->Map(0, &nothing_read, &index_mapped));
		std::memcpy(index_mapped, index_data.data(),
			static_cast<size_t>(index_bytes));
		index_upload->Unmap(0, nullptr);

		ID3D12GraphicsCommandList* list = this->open_frame_list();
		list->CopyBufferRegion(this->indices.Get(), 0, index_upload.Get(), 0,
			index_bytes);

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = this->indices.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter =
			D3D12_RESOURCE_STATE_INDEX_BUFFER;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		list->ResourceBarrier(1, &barrier);

		this->execute_frame_list();

		// The upload buffer is a local and is about to go, so the copy has to
		// have happened. This is device creation, not a frame.
		this->device_resources.wait_for_gpu();

		this->index_view.BufferLocation =
			this->indices->GetGPUVirtualAddress();
		this->index_view.SizeInBytes = static_cast<UINT>(index_bytes);
		this->index_view.Format = DXGI_FORMAT_R16_UINT;
	}

	void Renderer::Impl::on_device_lost()
	{
		this->device_info = {};

		for (std::unique_ptr<DrawList::View>& view : this->views)
		{
			view->list.Reset();
			for (int i = 0; i < DeviceResources::frame_count; i++)
			{
				view->allocators[i].Reset();
				view->pages[i].clear();
			}
			view->recording = false;
			view->batch.clear();
			view->batch_texture = nullptr;
			view->page = 0;
			view->page_position = 0;
		}

		this->frame_list.Reset();
		for (int i = 0; i < DeviceResources::frame_count; i++)
		{
			this->frame_allocators[i].Reset();
		}
		this->frame_list_open = false;

		this->indices.Reset();
		this->index_view = {};
		this->sprite_pipeline.reset();
		this->texture_heap.Reset();
		this->sampler_heap.Reset();
		this->next_texture_slot = 0;

		if (this->notify != nullptr)
		{
			this->notify->on_device_lost();
		}
	}

	void Renderer::Impl::on_device_restored()
	{
		this->create_device_dependent_resources();
		this->device_info = selected_device(this->device_resources.device());

		if (this->notify != nullptr)
		{
			this->notify->on_device_restored();
		}
	}

	// --- Renderer ------------------------------------------------------------

	Renderer::Renderer() : impl_(std::make_unique<Impl>())
	{

	}

	Renderer::~Renderer() = default;
	Renderer::Renderer(Renderer&&) noexcept = default;
	Renderer& Renderer::operator=(Renderer&&) noexcept = default;

	void Renderer::create_device(void* native_window, int width, int height,
		int view_capacity)
	{
		if (view_capacity < 1)
		{
			throw std::invalid_argument(
				"Renderer::create_device needs a view capacity of at least 1.");
		}

		this->impl_->device_resources.set_window(
			static_cast<HWND>(native_window), width, height);
		this->impl_->device_resources.create_device_resources();

		this->impl_->views.clear();
		this->impl_->views.reserve(static_cast<size_t>(view_capacity));
		this->impl_->submission_lists.clear();
		this->impl_->submission_lists.reserve(static_cast<size_t>(view_capacity));
		for (int i = 0; i < view_capacity; i++)
		{
			this->impl_->views.push_back(std::make_unique<DrawList::View>());
		}

		this->impl_->create_device_dependent_resources();
		this->impl_->device_resources.create_window_size_dependent_resources();
		this->impl_->device_info = selected_device(
			this->impl_->device_resources.device());
	}

	const RenderDeviceInfo& Renderer::device_info() const
	{
		if (this->impl_->device_info.backend.empty())
		{
			throw std::logic_error(
				"Renderer::device_info requires create_device.");
		}
		return this->impl_->device_info;
	}

	bool Renderer::window_size_changed(int width, int height)
	{
		Impl& impl = *this->impl_;

		// BEFORE THERE IS A DEVICE THERE IS NOTHING TO REBUILD, and the seam
		// says the answer is false (renderer.h). Reached by a shell that gets a
		// WM_SIZE between creating its window and creating its device, which is
		// an ordering Win32 allows and nothing here forbids -
		// tests/render/renderer_seam_tests.cpp holds all five backends to it.
		if (impl.device_resources.device() == nullptr)
		{
			return false;
		}

		// ASKED BEFORE ANYTHING IS THROWN AWAY, because most calls to this
		// change nothing and a frame is not worth losing to one of them.
		// Application::on_window_moved calls this with the size it already
		// has on every move of the window outside a drag - inside one,
		// app/win32/window.cpp gates WM_MOVE the way it gates WM_SIZE, because
		// that argument is a swap chain's size here and the live client rect on
		// the GL backend - and the shell calls it again on WM_EXITSIZEMOVE with
		// a size the drag may well have returned to. The comparison is the one
		// DeviceResources::window_size_changed makes itself; it is repeated
		// here rather than reached for through a new accessor because this is
		// the only caller and the alternative is a method whose whole purpose
		// is to be asked twice.
		const RECT size = impl.device_resources.output_size();
		if (static_cast<long>(width) == size.right - size.left &&
			static_cast<long>(height) == size.bottom - size.top)
		{
			return false;
		}

		// A FRAME IN PROGRESS IS RESTARTED, NOT REFUSED, and renderer.h states
		// that as a term of the seam rather than leaving it to a backend. What
		// every view has recorded names the render target of a back buffer
		// that the resize below destroys, and executing any of it afterwards is
		// a dead process rather than an error - which is what this did before
		// the rule was written down.
		//
		// IT HAS TO HAPPEN BEFORE THE RESIZE AND NOT AFTER IT. A closed list
		// is not the problem; an OPEN one is, because the resize waits for the
		// GPU and then resets allocators that a recording list is still
		// holding.
		//
		// AND WHETHER THERE IS A FRAME IS A THING THE FRAME SAYS, not a thing
		// the command lists are asked. frame_open() - is anything recording
		// right now - is false for the whole interval between begin_frame and
		// the first set_view_count, so a resize arriving there rebuilds the
		// buffer and then leaves it to be drawn into with no barrier and no
		// clear. Impl::frame_begun is the interval renderer.h actually names.
		const bool restart = impl.frame_begun;
		impl.abandon_recording();

		const bool rebuilt =
			impl.device_resources.window_size_changed(width, height);

		if (rebuilt && restart)
		{
			// Cleared and reopened against the buffer that now exists, so a
			// DrawList the caller is still holding draws into this frame
			// instead of into a resource that has gone. With no views declared
			// yet it is the barrier and the clear alone, which is exactly what
			// the frame is owed at that point.
			impl.open_frame();
		}

		return rebuilt;
	}

	void Renderer::set_device_notify(DeviceNotify* device_notify)
	{
		this->impl_->notify = device_notify;
	}

	void Renderer::set_resources(const RenderResources* resources)
	{
		this->impl_->resources = resources;
	}

	void Renderer::begin_frame()
	{
		Impl& impl = *this->impl_;

		// BEFORE ANYTHING RESETS AN ALLOCATOR, which is the whole of what this
		// backend adds to the seam's frame. Nothing below this line is safe
		// until the GPU has finished with the frame that used these allocators
		// two frames ago - and nothing above the seam knows or needs to.
		impl.device_resources.wait_for_frame();

		// A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT
		// one (renderer.h), and here that means closing what was left open.
		// The five backends have three different things to drop: three clear a
		// vector, the D3D11 one drains a deferred context, and this one has a
		// command list that is still recording - which cannot be reset, and
		// whose allocator cannot be reset under it either. Closing it throws
		// the commands away, because nothing will ever execute a list that
		// begin() is about to reset.
		impl.abandon_recording();

		// Before the clear, so that open_frame opens no views: this frame has
		// not said how many it has. The resize path calls the same function
		// with a count already set, which is the whole difference between
		// starting a frame and restarting one.
		impl.view_count = 0;

		// SET BEFORE THE CLEAR AND NOT AFTER IT, so that the frame owns
		// everything open_frame is about to do to the back buffer. From here
		// until end_frame, window_size_changed has a frame to restart.
		impl.frame_begun = true;
		impl.frame_submitted = false;

		impl.open_frame();
	}

	void Renderer::end_frame()
	{
		this->impl_->transition_back_buffer(D3D12_RESOURCE_STATE_PRESENT);
		this->impl_->execute_frame_list();

		// Cleared before the present rather than after it, because present()
		// is where a device loss surfaces and what it does about one is
		// rebuild every resource this frame was drawn with. There is nothing
		// left to restart by then.
		this->impl_->frame_begun = false;

		this->impl_->device_resources.present();
	}

	void Renderer::set_view_count(int count)
	{
		const int capacity = static_cast<int>(this->impl_->views.size());
		if (count < 0 || count > capacity)
		{
			throw std::out_of_range("Renderer::set_view_count(" +
				std::to_string(count) + ") is outside the capacity of " +
				std::to_string(capacity) + " create_device was given.");
		}

		// Lowering it past a view something has already drawn into strands that
		// recording: submit() would not execute it. Say so here rather than let
		// it turn up as one pane's worth of nothing.
		for (int i = count; i < this->impl_->view_count; i++)
		{
			if (this->impl_->views[static_cast<size_t>(i)]->touched)
			{
				throw std::logic_error("Renderer::set_view_count(" +
					std::to_string(count) + ") would drop view " +
					std::to_string(i) + ", which has already been drawn into "
					"this frame.");
			}
		}

		this->impl_->view_count = count;

		// Open what the frame is going to draw into, and nothing else. Views
		// already recording are left alone, so declaring the count twice is
		// free and does not overwrite a viewport set_viewport has since chosen.
		const D3D12_CPU_DESCRIPTOR_HANDLE render_target =
			this->impl_->device_resources.back_buffer_view();
		const D3D12_VIEWPORT viewport =
			this->impl_->device_resources.screen_viewport();

		for (int i = 0; i < count; i++)
		{
			this->impl_->views[static_cast<size_t>(i)]->begin(render_target,
				viewport);
		}
	}

	int Renderer::view_count() const
	{
		return this->impl_->view_count;
	}

	DrawList Renderer::view(int index) const
	{
		if (index < 0 || index >= this->impl_->view_count)
		{
			throw std::out_of_range("Renderer::view(" + std::to_string(index) +
				") - this frame has " + std::to_string(this->impl_->view_count) +
				" views. Asking for one nobody set up used to answer with a "
				"fullscreen pane and draw a whole extra pass.");
		}
		return DrawList(this->impl_->views[static_cast<size_t>(index)].get());
	}

	void Renderer::submit()
	{
		// ONCE PER FRAME, AND A SECOND CALL ADDS NOTHING (renderer.h). Cleared
		// by begin_frame, which is the only thing that starts a frame.
		if (this->impl_->frame_submitted)
		{
			return;
		}
		this->impl_->frame_submitted = true;

		// ONE CALL FOR THE WHOLE FRAME, WHICH IS THIS API'S SHAPE. The D3D11
		// backend finishes a command list per view and executes them one at a
		// time; here the finished lists are an array the queue takes in one go,
		// in view order - the only ordering guarantee the seam makes.
		//
		// EVERY RECORDING VIEW IS CLOSED; ONLY THE DECLARED ONES ARE EXECUTED,
		// and the two sets are not always the same one. Lowering the view count
		// past a view that was opened and not drawn into is legal -
		// set_view_count rejects only the ones that were drawn into - and a
		// list left open cannot have its allocator reset next frame.
		std::vector<ID3D12CommandList*>& lists = this->impl_->submission_lists;
		lists.clear();

		const int capacity = static_cast<int>(this->impl_->views.size());
		for (int i = 0; i < capacity; i++)
		{
			DrawList::View& view = *this->impl_->views[static_cast<size_t>(i)];
			if (!view.recording)
			{
				continue;
			}

			view.close();
			if (i < this->impl_->view_count)
			{
				lists.push_back(view.list.Get());
			}
		}

		if (lists.empty())
		{
			return;
		}

		this->impl_->device_resources.command_queue()->ExecuteCommandLists(
			static_cast<UINT>(lists.size()), lists.data());
		lists.clear();
		this->impl_->device_resources.signal_frame();
	}

	Vector2F Renderer::back_buffer_size() const
	{
		const RECT size = this->impl_->device_resources.output_size();
		return { static_cast<float>(size.right - size.left),
			static_cast<float>(size.bottom - size.top) };
	}

	void Renderer::read_back_buffer(std::vector<unsigned char>& pixels)
	{
		Impl& impl = *this->impl_;
		DeviceResources& device_resources = impl.device_resources;
		ID3D12Device* device = device_resources.device();

		ID3D12Resource* back_buffer = device_resources.back_buffer();
		const D3D12_RESOURCE_DESC description = back_buffer->GetDesc();

		// WHAT A ROW OF THE COPY IS, ASKED RATHER THAN COMPUTED. A copy out of
		// a texture lands in a buffer whose rows are padded to 256 bytes, and
		// the runtime is the only thing that knows to what - the same problem
		// the D3D11 backend has with a staging texture's RowPitch, one step
		// more explicit.
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT rows = 0;
		UINT64 row_bytes = 0;
		UINT64 total_bytes = 0;
		device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, &rows,
			&row_bytes, &total_bytes);

		// Made per call and thrown away: this is not a frame-path function and
		// a cached readback buffer would be one more thing to remake on a
		// device loss.
		ComPtr<ID3D12Resource> readback = create_buffer(device,
			D3D12_HEAP_TYPE_READBACK, total_bytes,
			D3D12_RESOURCE_STATE_COPY_DEST);

		const D3D12_RESOURCE_STATES previous =
			device_resources.back_buffer_state();
		impl.transition_back_buffer(D3D12_RESOURCE_STATE_COPY_SOURCE);

		D3D12_TEXTURE_COPY_LOCATION destination = {};
		destination.pResource = readback.Get();
		destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		destination.PlacedFootprint = footprint;

		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource = back_buffer;
		source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		source.SubresourceIndex = 0;

		impl.open_frame_list()->CopyTextureRegion(&destination, 0, 0, 0,
			&source, nullptr);

		// PUT BACK WHERE IT WAS, not into a state this function decided. The
		// seam says this is called between submit() and end_frame(), so the
		// back buffer is a render target and the frame is not over; a caller
		// that reads twice, or that reads and then presents, gets what it would
		// have got without the read.
		impl.transition_back_buffer(previous);
		impl.execute_frame_list();

		// It stalls on the GPU by construction, and the seam says so.
		device_resources.wait_for_gpu();

		const size_t width = static_cast<size_t>(description.Width);
		const size_t height = static_cast<size_t>(description.Height);
		pixels.resize(width * height * 4);

		void* mapped = nullptr;
		const D3D12_RANGE everything = { 0, static_cast<SIZE_T>(total_bytes) };
		ThrowIfFailed(readback->Map(0, &everything, &mapped));

		// B and R swap on the way out: the back buffer is B8G8R8A8 and the seam
		// promises RGBA.
		const unsigned char* bytes = static_cast<const unsigned char*>(mapped);
		for (size_t y = 0; y < height; y++)
		{
			const unsigned char* row = bytes +
				y * static_cast<size_t>(footprint.Footprint.RowPitch);
			for (size_t x = 0; x < width; x++)
			{
				unsigned char* out = pixels.data() + (y * width + x) * 4;
				out[0] = row[x * 4 + 2];
				out[1] = row[x * 4 + 1];
				out[2] = row[x * 4 + 0];
				out[3] = row[x * 4 + 3];
			}
		}

		const D3D12_RANGE nothing_written = { 0, 0 };
		readback->Unmap(0, &nothing_written);
	}

	// THE THREE MARKERS DO NOTHING, AND THAT IS THE HONEST ANSWER RATHER THAN A
	// GAP. D3D11 has ID3DUserDefinedAnnotation, which is part of the API; the
	// D3D12 equivalent is an opaque blob whose encoding belongs to
	// WinPixEventRuntime, a library this repository would have to buy to write
	// three no-op wrappers (T9). The GL backend's markers do nothing for the
	// same kind of reason and say so in the same place.
	void Renderer::begin_marker(const wchar_t* name)
	{
		std::ignore = name;
	}

	void Renderer::end_marker()
	{
	}

	void Renderer::set_marker(const wchar_t* name)
	{
		std::ignore = name;
	}
}

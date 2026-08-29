#include "engine/render/resource_factory.h"

#include "engine/render/d3d12/backend.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/render/throw_if_failed.h"

#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace labrador
{
	namespace
	{
		// Read fresh on every call rather than held, for the reason the D3D11
		// backend's copy of this function gives: a cached device is a caller
		// obligation nobody stated, and asking is one member read on the load
		// path.
		ID3D12Device* device_of(const Renderer& renderer)
		{
			return renderer.impl()->device_resources.device();
		}

		// The one hand-written message in this file that reports an HRESULT,
		// spelt the way com_exception spells one so that a search for a code
		// finds both. Not put beside that class, because throw_if_failed.h is
		// carried with upstream's naming (its own header says why) and this is
		// the engine's own sentence.
		std::string hresult_name(HRESULT hr)
		{
			char text[24] = {};
			sprintf_s(text, "HRESULT %08X", static_cast<unsigned int>(hr));
			return text;
		}

		// The engine's format vocabulary, back into this API's. The other
		// direction is in dds_file.cpp and sprite_font_file.cpp, where files
		// that spell their format as a fourCC or a DXGI number are decoded.
		//
		// IDENTICAL TO THE D3D11 BACKEND'S SWITCH, AND NOT SHARED WITH IT.
		// DXGI_FORMAT is a type from a header this folder's neighbour may not
		// include and this one may - which is exactly the wall
		// cmake/check_engine_includes.cmake enforces. A common file naming
		// DXGI_FORMAT would have to live outside both folders, which is the one
		// place a DXGI number may not be named at all (texture_format.h says
		// so). Six lines of duplication is the price of that rule, and it is
		// the rule that makes a backend a folder rather than a convention.
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

		// For the throw, because an eight-digit HRESULT is not an answer (T6).
		const char* format_name(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::b8g8r8a8_unorm: return "b8g8r8a8_unorm";
			case TextureFormat::b4g4r4a4_unorm: return "b4g4r4a4_unorm";
			case TextureFormat::bc1_unorm:      return "bc1_unorm";
			case TextureFormat::bc2_unorm:      return "bc2_unorm";
			case TextureFormat::bc3_unorm:      return "bc3_unorm";
			case TextureFormat::r8g8b8a8_unorm:
			default:                            return "r8g8b8a8_unorm";
			}
		}
	}

	// The whole of this backend's share of loading content, and it is the
	// second longest of the five because this API has no initial-data
	// parameter. Vulkan's is longer still and for the same reason - 382 lines
	// to this one's 342, with gl at 168, d3d11 at 116 and null at 49, which
	// renderer.h lists in one place and holds all five against.
	//
	// WHAT A TEXTURE COSTS HERE THAT IT DOES NOT COST NEXT DOOR. D3D11 takes an
	// array of D3D11_SUBRESOURCE_DATA and is done - the runtime copies the
	// bytes wherever they have to go, and the caller never learns where that is
	// or when it happened. Here the engine does it: a resource in GPU memory, a
	// staging buffer in memory the CPU can write, a copy per mip level recorded
	// onto a command list, a barrier that says the copy is finished, and a wait
	// - because the staging buffer is a local and the GPU has to be done
	// reading it before it goes.
	//
	// A FULL STALL PER TEXTURE, AND THAT IS THE RIGHT ANSWER HERE RATHER THAN A
	// SHORTCUT - and it is the stall of the two backends that own their
	// uploads, which the sentence here has now had wrong in both directions.
	// Loading is a synchronous, blocking path on all five (resource_factory.h
	// says why it reads its file that way); this backend and vulkan/ wait on a
	// GPU inside it, because both record a copy and both own the staging buffer
	// it reads - vulkan/texture_factory.cpp calls end_upload for exactly this
	// reason. d3d11 hands the bytes to the runtime, gl hands them to the
	// driver, and null keeps them. It runs at load and never on the frame
	// path, and the alternative - keeping every upload buffer alive until some
	// later fence - is a pool and a lifetime rule for a path that reads files
	// off a disk.
	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture)
	{
		// BEFORE ANYTHING ELSE, because the ordering is a rule of the seam
		// (resource_factory.h) and the honest failure is a named throw rather
		// than a null dereference inside the runtime.
		if (device_of(renderer) == nullptr)
		{
			throw std::runtime_error("Texture '" + name + "' was loaded before "
				"create_device made a device.");
		}

		ID3D12Device* device = device_of(renderer);
		Renderer::Impl& impl = *renderer.impl();

		// ONE COUNT, DERIVED ONCE AND REFUSED IF IT WILL NOT FIT. Casting
		// texture.levels.size() twice at two different widths - UINT16 into
		// MipLevels below, UINT into GetCopyableFootprints after it - truncates
		// a count that does not fit the first rather than rejecting it, so
		// CreateCommittedResource succeeds on a number the file never said.
		// That is the one way past the named throw this function has for a
		// texture the device will not take, and it leaves GetCopyableFootprints
		// asked about a subresource count outside the range its annotation
		// gives.
		//
		// dds_file.cpp already bounds the count at what the dimensions can
		// produce, for every backend. This is the wall behind that one, and it
		// is this API's number rather than the engine's.
		if (texture.levels.size() > static_cast<size_t>(D3D12_REQ_MIP_LEVELS))
		{
			throw std::runtime_error("Texture '" + name + "' has " +
				std::to_string(texture.levels.size()) + " mip levels, and "
				"Direct3D 12 takes at most " +
				std::to_string(D3D12_REQ_MIP_LEVELS) + ".");
		}
		const UINT16 level_count =
			static_cast<UINT16>(texture.levels.size());

		D3D12_RESOURCE_DESC description = {};
		description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		description.Alignment = 0;
		description.Width = static_cast<UINT64>(texture.width);
		description.Height = static_cast<UINT>(texture.height);
		description.DepthOrArraySize = 1;
		description.MipLevels = level_count;
		description.Format = to_dxgi_format(texture.format);
		description.SampleDesc.Count = 1;
		description.SampleDesc.Quality = 0;
		description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		description.Flags = D3D12_RESOURCE_FLAG_NONE;

		const D3D12_HEAP_PROPERTIES default_heap = { D3D12_HEAP_TYPE_DEFAULT,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

		ComPtr<ID3D12Resource> resource;
		const HRESULT created = device->CreateCommittedResource(&default_heap,
			D3D12_HEAP_FLAG_NONE, &description,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(resource.GetAddressOf()));
		if (FAILED(created))
		{
			// THE ANSWER A BACKEND OWES WHEN IT WILL NOT TAKE A FORMAT, and on
			// this one the format most likely to arrive here is b4g4r4a4_unorm:
			// D3D12 makes 4-bit-per-channel support optional, so a device that
			// takes every other entry in texture_format.h can refuse that one.
			// No file in either client uses it.
			//
			// AND THE CODE, BECAUSE THE SENTENCE ASSERTS A CAUSE AND ONLY ONE
			// OF FOUR ANSWERS HAS IT. CreateCommittedResource returns
			// E_OUTOFMEMORY for a heap it cannot make, E_INVALIDARG for a
			// description it will not parse and DXGI_ERROR_DEVICE_REMOVED for a
			// device that has gone, so reporting all three as an unsupported
			// format at a size asserts a cause the code has not established. The
			// Vulkan factory next door names its VkResult in the same position and
			// for the same reason; both Direct3D backends have com_exception's
			// "Failure with HRESULT of" available at every ThrowIfFailed site, so
			// there is no reason to assert a cause and throw the evidence away
			// (T6).
			throw std::runtime_error("Texture '" + name + "' is " +
				format_name(texture.format) + " at " +
				std::to_string(texture.width) + "x" +
				std::to_string(texture.height) +
				", which this device will not take (" +
				hresult_name(created) + ").");
		}

		// WHERE EACH LEVEL GOES IN THE STAGING BUFFER, ASKED RATHER THAN
		// COMPUTED. A copy's rows are padded to 256 bytes and each level starts
		// on a 512-byte boundary, and neither number is this engine's to assume
		// - which matters most for the block-compressed levels that are 41 of
		// the 43 images both clients load between them (docs/port/content-
		// probe.md counted them), where a "row" is a row of 4x4 blocks and the
		// obvious arithmetic is four times too large (texture_data.h).
		const UINT subresources = level_count;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
			subresources);
		std::vector<UINT> row_counts(subresources);
		std::vector<UINT64> row_bytes(subresources);
		UINT64 total_bytes = 0;
		device->GetCopyableFootprints(&description, 0, subresources, 0,
			footprints.data(), row_counts.data(), row_bytes.data(),
			&total_bytes);

		const D3D12_HEAP_PROPERTIES upload_heap = { D3D12_HEAP_TYPE_UPLOAD,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

		D3D12_RESOURCE_DESC staging_description = {};
		staging_description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		staging_description.Width = total_bytes;
		staging_description.Height = 1;
		staging_description.DepthOrArraySize = 1;
		staging_description.MipLevels = 1;
		staging_description.Format = DXGI_FORMAT_UNKNOWN;
		staging_description.SampleDesc.Count = 1;
		staging_description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		staging_description.Flags = D3D12_RESOURCE_FLAG_NONE;

		ComPtr<ID3D12Resource> staging;
		ThrowIfFailed(device->CreateCommittedResource(&upload_heap,
			D3D12_HEAP_FLAG_NONE, &staging_description,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(staging.GetAddressOf())));

		void* mapped = nullptr;
		const D3D12_RANGE nothing_read = { 0, 0 };
		ThrowIfFailed(staging->Map(0, &nothing_read, &mapped));

		// ROW BY ROW, BECAUSE THE TWO STRIDES DIFFER. The engine's stride is
		// what the file has (TextureLevel::stride); the copy's is whatever
		// GetCopyableFootprints just said. They agree only by accident.
		unsigned char* staging_bytes = static_cast<unsigned char*>(mapped);
		for (UINT level = 0; level < subresources; level++)
		{
			const TextureLevel& source = texture.levels[level];
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& target =
				footprints[level];

			for (UINT row = 0; row < row_counts[level]; row++)
			{
				std::memcpy(
					staging_bytes + target.Offset +
						static_cast<size_t>(row) * target.Footprint.RowPitch,
					texture.pixels.data() + source.offset +
						static_cast<size_t>(row) *
							static_cast<size_t>(source.stride),
					static_cast<size_t>(row_bytes[level]));
			}
		}

		staging->Unmap(0, nullptr);

		// ON THE FRAME LIST, WHICH IS THE ONE THIS BACKEND HAS THAT IS NOT A
		// VIEW'S. A view's list is for drawing and there may be none of them
		// open; this one exists for exactly the work that belongs to the
		// renderer rather than to a pane (backend.h).
		ID3D12GraphicsCommandList* list = impl.open_frame_list();

		for (UINT level = 0; level < subresources; level++)
		{
			D3D12_TEXTURE_COPY_LOCATION destination = {};
			destination.pResource = resource.Get();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			destination.SubresourceIndex = level;

			D3D12_TEXTURE_COPY_LOCATION source = {};
			source.pResource = staging.Get();
			source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			source.PlacedFootprint = footprints[level];

			list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
		}

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter =
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		list->ResourceBarrier(1, &barrier);

		impl.execute_frame_list();
		impl.device_resources.wait_for_gpu();

		// The descriptor, which is what a draw call actually names. Claimed
		// last, so a texture the device refused never takes a slot.
		//
		// AND THE NAME'S OWN SLOT IF IT ALREADY HAS ONE, which is the whole of
		// what re-loading a texture costs this backend. registry.h and
		// render_resources.h both make re-adding a name a first-class operation
		// - "Re-adding a name reuses its slot" - and Registry::add duly replaces
		// the entry and destroys the old D3d12Texture. Its slot number is a
		// plain int with no destructor, so allocating a second one per re-load
		// spends the heap on textures that are no longer live: a client
		// re-walking a manifest per level runs out of a 256-entry heap with a few
		// dozen textures in it, and a single manifest naming one asset as both a
		// texture and a sprite sheet does it inside one walk.
		//
		// OVERWRITING A SHADER-VISIBLE DESCRIPTOR IS SAFE HERE AND NOWHERE ELSE
		// IN THIS FILE, which is the obvious objection and worth answering: the
		// execute and the wait immediately above leave the GPU idle, so no list
		// in flight is naming this slot.
		const int existing = resources.impl()->descriptor_slot(name);
		const int slot = existing >= 0
			? existing
			: impl.allocate_texture_slot(name);

		// A null description means "the whole resource as it was created",
		// which is what every texture this engine loads wants - all of its
		// levels, in its own format. Whether a minified draw ever reads a
		// second level is the sampler's business and the answer is no
		// (renderer.cpp, MaxLOD).
		device->CreateShaderResourceView(resource.Get(), nullptr,
			impl.texture_slot_cpu(slot));

		resources.impl()->add_texture(name,
			std::make_unique<D3d12Texture>(resource, slot, texture.width,
				texture.height));
	}
}

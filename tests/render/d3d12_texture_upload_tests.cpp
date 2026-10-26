#include "engine/render/d3d12/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/throw_if_failed.h"
#include "tests/render/device_fixture.h"

#include <doctest/doctest.h>
#include <d3d12sdklayers.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	using namespace labrador;
	using Microsoft::WRL::ComPtr;

	TextureData patterned_chain(TextureFormat format, int width, int height)
	{
		TextureData texture;
		texture.width = width;
		texture.height = height;
		texture.format = format;
		for (;;)
		{
			const TextureLevel level = texture_level(format, width, height,
				texture.pixels.size());
			const size_t index = texture.levels.size();
			texture.levels.push_back(level);
			texture.pixels.resize(level.offset + level.size);
			for (int row = 0; row < level.rows; ++row)
			{
				for (int column = 0; column < level.stride; ++column)
				{
					texture.pixels[level.offset + static_cast<size_t>(row) *
						static_cast<size_t>(level.stride) + static_cast<size_t>(column)] =
						static_cast<unsigned char>((index * 61 +
							static_cast<size_t>(row) * 29 +
							static_cast<size_t>(column) * 17 + 7) % 256);
				}
			}
			if (width == 1 && height == 1) return texture;
			width = (std::max)(1, width / 2);
			height = (std::max)(1, height / 2);
		}
	}

	void compare_uploaded_chain(Renderer& renderer, ID3D12Resource* texture,
		const TextureData& expected)
	{
		Renderer::Impl& impl = *renderer.impl();
		ID3D12Device* device = impl.device_resources.device();
		const D3D12_RESOURCE_DESC description = texture->GetDesc();
		const UINT level_count = static_cast<UINT>(expected.levels.size());
		REQUIRE(description.MipLevels == level_count);
		REQUIRE(description.Width == static_cast<UINT64>(expected.width));
		REQUIRE(description.Height == static_cast<UINT>(expected.height));
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(level_count);
		std::vector<UINT> row_counts(level_count);
		std::vector<UINT64> row_bytes(level_count);
		UINT64 total_bytes = 0;
		device->GetCopyableFootprints(&description, 0, level_count, 0,
			footprints.data(), row_counts.data(), row_bytes.data(), &total_bytes);
		REQUIRE(total_bytes > 0);
		bool has_row_padding = false;
		for (UINT level = 0; level < level_count; ++level)
		{
			CAPTURE(level);
			const TextureLevel& source = expected.levels[level];
			REQUIRE(row_counts[level] == static_cast<UINT>(source.rows));
			REQUIRE(row_bytes[level] == static_cast<UINT64>(source.stride));
			REQUIRE(footprints[level].Footprint.RowPitch >= row_bytes[level]);
			REQUIRE(footprints[level].Offset +
				static_cast<UINT64>(source.rows - 1) *
				footprints[level].Footprint.RowPitch + row_bytes[level] <= total_bytes);
			has_row_padding = has_row_padding ||
				footprints[level].Footprint.RowPitch > row_bytes[level];
			if (level > 0)
			{
				REQUIRE(footprints[level].Offset > footprints[level - 1].Offset);
			}
		}
		REQUIRE(has_row_padding);

		const D3D12_HEAP_PROPERTIES heap = { D3D12_HEAP_TYPE_READBACK,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
		D3D12_RESOURCE_DESC buffer = {};
		buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer.Width = total_bytes;
		buffer.Height = 1;
		buffer.DepthOrArraySize = 1;
		buffer.MipLevels = 1;
		buffer.SampleDesc.Count = 1;
		buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ComPtr<ID3D12Resource> readback;
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
			&buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(readback.GetAddressOf())));

		// The real factory has waited for upload completion and left every
		// subresource shader-readable. Restore that state before returning.
		ID3D12GraphicsCommandList* list = impl.open_frame_list();
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		list->ResourceBarrier(1, &barrier);
		for (UINT level = 0; level < level_count; ++level)
		{
			D3D12_TEXTURE_COPY_LOCATION source = {};
			source.pResource = texture;
			source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			source.SubresourceIndex = level;
			D3D12_TEXTURE_COPY_LOCATION destination = {};
			destination.pResource = readback.Get();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			destination.PlacedFootprint = footprints[level];
			// A whole-subresource copy also covers BC tail levels below 4x4.
			list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
		}
		std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		list->ResourceBarrier(1, &barrier);
		impl.execute_frame_list();
		impl.device_resources.wait_for_gpu();

		void* mapped = nullptr;
		const D3D12_RANGE read_range = { 0, static_cast<SIZE_T>(total_bytes) };
		ThrowIfFailed(readback->Map(0, &read_range, &mapped));
		const unsigned char* bytes = static_cast<const unsigned char*>(mapped);
		for (UINT level = 0; level < level_count; ++level)
		{
			CAPTURE(level);
			const TextureLevel& source = expected.levels[level];
			for (UINT row = 0; row < row_counts[level]; ++row)
			{
				CAPTURE(row);
				const unsigned char* actual = bytes + footprints[level].Offset +
					static_cast<size_t>(row) * footprints[level].Footprint.RowPitch;
				const unsigned char* wanted = expected.pixels.data() + source.offset +
					static_cast<size_t>(row) * static_cast<size_t>(source.stride);
				CHECK(std::memcmp(actual, wanted, static_cast<size_t>(source.stride)) == 0);
			}
		}
		const D3D12_RANGE nothing_written = { 0, 0 };
		readback->Unmap(0, &nothing_written);
	}

	void check_debug_messages(ID3D12InfoQueue* messages)
	{
		if (messages == nullptr)
		{
			INFO("D3D12 debug layer unavailable; byte comparisons still ran");
			WARN(messages != nullptr);
			return;
		}
		const UINT64 count = messages->GetNumStoredMessagesAllowedByRetrievalFilter();
		for (UINT64 index = 0; index < count; ++index)
		{
			SIZE_T bytes = 0;
			ThrowIfFailed(messages->GetMessage(index, nullptr, &bytes));
			std::vector<D3D12_MESSAGE> storage(
				(bytes + sizeof(D3D12_MESSAGE) - 1) / sizeof(D3D12_MESSAGE));
			D3D12_MESSAGE* message = storage.data();
			ThrowIfFailed(messages->GetMessage(index, message, &bytes));
			INFO(message->pDescription);
			CHECK(message->Severity > D3D12_MESSAGE_SEVERITY_WARNING);
		}
	}
}

TEST_CASE("D3D12 uploads every patterned RGBA and BC mip including block tails")
{
	render_tests::DeviceFixture fixture;
	ComPtr<ID3D12InfoQueue> messages;
	if (SUCCEEDED(fixture.renderer.impl()->device_resources.device()->QueryInterface(
		IID_PPV_ARGS(messages.GetAddressOf()))))
	{
		messages->ClearStoredMessages();
	}
	for (TextureFormat format : { TextureFormat::r8g8b8a8_unorm,
		TextureFormat::bc1_unorm, TextureFormat::bc2_unorm, TextureFormat::bc3_unorm })
	{
		CAPTURE(static_cast<int>(format));
		const bool compressed = is_block_compressed(format);
		const TextureData texture = patterned_chain(format,
			compressed ? 20 : 17, compressed ? 12 : 9);
		REQUIRE(texture.levels.size() == 5);
		CHECK(texture.levels.back().width == 1);
		CHECK(texture.levels.back().height == 1);
		if (compressed)
		{
			CHECK(texture.levels[3].width == 2);
			CHECK(texture.levels[3].height == 1);
			CHECK(texture.levels[3].rows == 1);
			CHECK(texture.levels[4].rows == 1);
		}
		const std::string name = "mip_readback_" + std::to_string(static_cast<int>(format));
		add_texture_asset(fixture.renderer, fixture.resources, name, texture);
		const TextureHandle handle = fixture.resources.resolve_texture(name);
		const D3d12Texture* uploaded = fixture.resources.impl()->texture(handle);
		REQUIRE(uploaded != nullptr);
		REQUIRE(uploaded->resource() != nullptr);
		compare_uploaded_chain(fixture.renderer, uploaded->resource(), texture);
	}
	check_debug_messages(messages.Get());
}

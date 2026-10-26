#include "engine/render/d3d12/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/throw_if_failed.h"
#include "tests/render/test_window.h"

#include <doctest/doctest.h>
#include <d3d12sdklayers.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace
{
	using namespace labrador;
	using namespace mattmath;
	using Microsoft::WRL::ComPtr;

	constexpr int initial_size = 64;
	constexpr int resized_size = 32;
	constexpr int presented_frames = DeviceResources::FRAME_COUNT * 2 + 2;
	using SlotVisits = std::array<int, DeviceResources::FRAME_COUNT>;

	struct Fixture
	{
		render_tests::TestWindow window{ initial_size, initial_size };
		RenderResources resources;
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();

		Fixture()
		{
			renderer->create_device(window.handle(), initial_size, initial_size, 2);
			renderer->set_resources(&resources);
		}
	};

	ComPtr<ID3D12InfoQueue> diagnostic_queue(Renderer& renderer)
	{
		ComPtr<ID3D12InfoQueue> messages;
		const HRESULT result = renderer.impl()->device_resources.device()->QueryInterface(
			IID_PPV_ARGS(messages.GetAddressOf()));
		REQUIRE_MESSAGE(SUCCEEDED(result),
			"D3D12 diagnostic acceptance requires an available Debug InfoQueue");
		REQUIRE(messages != nullptr);
		return messages;
	}

	class QueueFilters
	{
	public:
		explicit QueueFilters(ID3D12InfoQueue& messages) : messages_(messages)
		{
			ThrowIfFailed(messages.PushEmptyStorageFilter());
			try
			{
				ThrowIfFailed(messages.PushEmptyRetrievalFilter());
			}
			catch (...)
			{
				messages.PopStorageFilter();
				throw;
			}
		}
		~QueueFilters()
		{
			this->messages_.PopRetrievalFilter();
			this->messages_.PopStorageFilter();
		}
		QueueFilters(const QueueFilters&) = delete;
		QueueFilters& operator=(const QueueFilters&) = delete;

	private:
		ID3D12InfoQueue& messages_;
	};

	std::vector<D3D12_MESSAGE> read_message(ID3D12InfoQueue& messages, UINT64 index)
	{
		SIZE_T bytes = 0;
		ThrowIfFailed(messages.GetMessage(index, nullptr, &bytes));
		REQUIRE(bytes >= sizeof(D3D12_MESSAGE));
		std::vector<D3D12_MESSAGE> storage(
			(bytes + sizeof(D3D12_MESSAGE) - 1) / sizeof(D3D12_MESSAGE));
		ThrowIfFailed(messages.GetMessage(index, storage.data(), &bytes));
		return storage;
	}

	struct QueueCounts
	{
		UINT64 denied = 0;
		UINT64 discarded = 0;
	};

	QueueCounts calibrate(ID3D12InfoQueue& messages)
	{
		// Initialization preceding this clear is outside the workload capture.
		messages.ClearStoredMessages();
		const char* const marker = "Labrador presentation diagnostic calibration";
		ThrowIfFailed(messages.AddApplicationMessage(D3D12_MESSAGE_SEVERITY_INFO, marker));
		REQUIRE(messages.GetNumStoredMessages() == 1);
		REQUIRE(messages.GetNumStoredMessagesAllowedByRetrievalFilter() == 1);
		const std::vector<D3D12_MESSAGE> storage = read_message(messages, 0);
		CHECK(storage[0].Severity == D3D12_MESSAGE_SEVERITY_INFO);
		REQUIRE(storage[0].pDescription != nullptr);
		CHECK(std::string(storage[0].pDescription) == std::string(marker));
		CHECK(messages.GetNumMessagesDiscardedByMessageCountLimit() == 0);
		messages.ClearStoredMessages();
		REQUIRE(messages.GetNumStoredMessages() == 0);
		// These counters include initialization and survive the clear.
		return { messages.GetNumMessagesDeniedByStorageFilter(),
			messages.GetNumMessagesDiscardedByMessageCountLimit() };
	}

	void check_diagnostics(ID3D12InfoQueue& messages, QueueCounts baseline)
	{
		const UINT64 stored = messages.GetNumStoredMessages();
		CAPTURE(stored);
		CAPTURE(baseline.denied);
		CHECK(messages.GetNumStoredMessagesAllowedByRetrievalFilter() == stored);
		CHECK(messages.GetNumMessagesDeniedByStorageFilter() == baseline.denied);
		CHECK(messages.GetNumMessagesDiscardedByMessageCountLimit() == baseline.discarded);
		for (UINT64 index = 0; index < stored; ++index)
		{
			const std::vector<D3D12_MESSAGE> storage = read_message(messages, index);
			const D3D12_MESSAGE& message = storage[0];
			INFO(std::string(message.pDescription == nullptr ? "no description" : message.pDescription));
			CHECK(message.Severity > D3D12_MESSAGE_SEVERITY_WARNING);
		}
	}

	TextureData white_texture()
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size, 255);
		return texture;
	}

	SlotVisits present_frames(Renderer& renderer, TextureHandle texture, int first_frame)
	{
		SlotVisits visits{};
		for (int frame = first_frame; frame < first_frame + presented_frames; ++frame)
		{
			CAPTURE(frame);
			renderer.begin_frame();
			const int slot = renderer.impl()->device_resources.frame_index();
			REQUIRE(slot >= 0);
			REQUIRE(slot < DeviceResources::FRAME_COUNT);
			++visits[static_cast<size_t>(slot)];
			renderer.set_view_count(2);
			for (int view = 0; view < 2; ++view)
			{
				DrawList list = renderer.view(view);
				// Two full vertex pages and a partial third in each view.
				for (int sprite = 0; sprite < 4097; ++sprite)
				{
					list.draw_sprite(texture, RectangleI(0, 0, 2, 2),
						RectangleF(static_cast<float>((sprite + frame) % 8) * 2.0f,
							static_cast<float>((sprite / 8 + view) % 8) * 2.0f, 2.0f, 2.0f),
						frame % 2 == 0 ? Colour::red : Colour::blue,
						0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
				}
			}
			renderer.submit();
			renderer.end_frame();
		}
		return visits;
	}
}

TEST_CASE("D3D12 presentation diagnostics retrieve an exact calibration message")
{
	Fixture fixture;
	ComPtr<ID3D12InfoQueue> messages = diagnostic_queue(*fixture.renderer);
	QueueFilters filters(*messages.Get());
	const QueueCounts baseline = calibrate(*messages.Get());
	fixture.renderer.reset();
	fixture.resources.release_device_resources();
	check_diagnostics(*messages.Get(), baseline);
}

TEST_CASE("D3D12 presentation diagnostics cover reuse resize and engine resource teardown")
{
	Fixture fixture;
	ComPtr<ID3D12InfoQueue> messages = diagnostic_queue(*fixture.renderer);
	QueueFilters filters(*messages.Get());
	const QueueCounts baseline = calibrate(*messages.Get());
	add_texture_asset(*fixture.renderer, fixture.resources,
		"presentation_diagnostics", white_texture());
	const TextureHandle texture = fixture.resources.resolve_texture("presentation_diagnostics");
	const SlotVisits before = present_frames(*fixture.renderer, texture, 0);
	REQUIRE(fixture.window.resize(resized_size, resized_size));
	CHECK(fixture.renderer->window_size_changed(resized_size, resized_size));
	CHECK(fixture.renderer->back_buffer_size() == Vector2F(
		static_cast<float>(resized_size), static_cast<float>(resized_size)));
	const SlotVisits after = present_frames(*fixture.renderer, texture, presented_frames);
	CAPTURE(before[0]);
	CAPTURE(before[1]);
	CAPTURE(after[0]);
	CAPTURE(after[1]);
	CHECK(*std::max_element(before.begin(), before.end()) >= 2);
	CHECK(*std::max_element(after.begin(), after.end()) >= 2);

	// No read-back or explicit wait: the renderer owns the teardown wait.
	fixture.renderer.reset();
	fixture.resources.release_device_resources();
	check_diagnostics(*messages.Get(), baseline);
	// The retained queue keeps the device alive. This is not final device
	// destruction coverage or proof that work was outstanding at a boundary.
}

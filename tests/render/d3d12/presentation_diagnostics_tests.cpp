#include <doctest/doctest.h>
#include "engine/render/d3d12/backend.h"
#include "engine/render/post_effect.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/throw_if_failed.h"
#include "tests/render/page_crossing_frame.h"
#include "tests/render/test_textures.h"
#include "tests/render/test_window.h"

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
	using Microsoft::WRL::ComPtr;

	constexpr int initial_size = 64;
	constexpr int resized_size = 32;
	constexpr int presented_frames = DeviceResources::frame_count * 2 + 2;
	using SlotVisits = std::array<int, DeviceResources::frame_count>;

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

	SlotVisits present_frames(Renderer& renderer, TextureHandle texture, int first_frame)
	{
		SlotVisits visits{};
		for (int frame = first_frame; frame < first_frame + presented_frames; ++frame)
		{
			CAPTURE(frame);
			renderer.begin_frame();
			const int slot = renderer.impl()->device_resources.frame_index();
			REQUIRE(slot >= 0);
			REQUIRE(slot < DeviceResources::frame_count);
			++visits[static_cast<size_t>(slot)];
			// Two full vertex pages and a partial third in each view.
			render_tests::record_page_crossing_views(renderer, texture, frame);
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
		"presentation_diagnostics", render_tests::flat_texture(255, 255, 255));
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

TEST_CASE("D3D12 presentation diagnostics cover the post stage's transitions, resize and removal")
{
	Fixture fixture;
	ComPtr<ID3D12InfoQueue> messages = diagnostic_queue(*fixture.renderer);
	QueueFilters filters(*messages.Get());
	const QueueCounts baseline = calibrate(*messages.Get());
	add_texture_asset(*fixture.renderer, fixture.resources,
		"presentation_diagnostics_post", render_tests::flat_texture(255, 255, 255));
	const TextureHandle texture =
		fixture.resources.resolve_texture("presentation_diagnostics_post");

	// The scene through identity, frames in flight on both slots, with an
	// overlay view after the pass, an abandoned frame and a zero-view frame.
	const PostEffect identity[] = { PostEffect::identity };
	fixture.renderer->set_post_effects(identity);
	present_frames(*fixture.renderer, texture, 0);

	fixture.renderer->begin_frame();
	render_tests::record_page_crossing_views(*fixture.renderer, texture, 1);
	fixture.renderer->begin_frame();
	fixture.renderer->set_view_count(1);
	fixture.renderer->begin_overlay();
	DrawList overlay = fixture.renderer->view(1);
	overlay.draw_sprite(texture, RectangleI(0, 0, 2, 2),
		RectangleF(4.0f, 4.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	fixture.renderer->submit();
	fixture.renderer->end_frame();

	fixture.renderer->begin_frame();
	fixture.renderer->submit();
	fixture.renderer->end_frame();

	// A resize remakes the scene, and the CRT draws through its own
	// pipeline state on the same root signature; removing the chain
	// releases the scene.
	REQUIRE(fixture.window.resize(resized_size, resized_size));
	CHECK(fixture.renderer->window_size_changed(resized_size, resized_size));
	present_frames(*fixture.renderer, texture, presented_frames);
	const PostEffect crt[] = { PostEffect::crt };
	fixture.renderer->set_post_effects(crt);
	CrtSettings settings;
	settings.screen = RectangleF(0.0f, 0.0f,
		static_cast<float>(resized_size), static_cast<float>(resized_size));
	settings.curvature = 0.2f;
	settings.scanline_count = 8.0f;
	settings.scanline_strength = 0.5f;
	settings.mask_strength = 0.5f;
	settings.vignette = 0.5f;
	settings.aberration = 1.0f;
	const PostValues crt_terms = crt_values(settings, Vector2F(
		static_cast<float>(resized_size), static_cast<float>(resized_size)));
	REQUIRE_FALSE(crt_values_are_neutral(crt_terms));
	fixture.renderer->set_post_values(PostEffect::crt, crt_terms);
	present_frames(*fixture.renderer, texture, 2 * presented_frames);
	fixture.renderer->set_post_effects({});
	present_frames(*fixture.renderer, texture, 3 * presented_frames);

	fixture.renderer.reset();
	fixture.resources.release_device_resources();
	check_diagnostics(*messages.Get(), baseline);
}

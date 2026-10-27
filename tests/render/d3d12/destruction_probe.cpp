#include "tests/render/test_window.h"

#include "engine/render/d3d12/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <d3d12sdklayers.h>
#include <d3dcommon.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#ifndef NDEBUG
namespace
{
	using namespace labrador;
	using namespace labrador;
	using Microsoft::WRL::ComPtr;

	constexpr std::size_t message_capacity = 128;
	constexpr std::size_t description_capacity = 512;
	constexpr int initial_size = 64;
	constexpr int resized_size = 32;
	constexpr char calibration_marker[] = "Labrador final-device callback calibration";
	constexpr char teardown_marker[] = "Labrador final-device callback before teardown";

	enum class Phase
	{
		calibration,
		frames,
		resize,
		before_teardown,
		renderer_release,
		texture_release,
		owners_released,
	};

	const char* phase_name(Phase phase)
	{
		switch (phase)
		{
		case Phase::calibration: return "calibration";
		case Phase::frames: return "frames";
		case Phase::resize: return "resize";
		case Phase::before_teardown: return "before-teardown";
		case Phase::renderer_release: return "renderer-release";
		case Phase::texture_release: return "texture-release";
		case Phase::owners_released: return "owners-released";
		}
		return "unknown";
	}

	struct Message
	{
		std::atomic<bool> ready{ false };
		D3D12_MESSAGE_CATEGORY category{};
		D3D12_MESSAGE_SEVERITY severity{};
		D3D12_MESSAGE_ID id{};
		Phase phase{};
		std::array<char, description_capacity> description{};
	};

	struct Capture
	{
		std::array<Message, message_capacity> messages;
		std::atomic<std::size_t> started{ 0 };
		std::atomic<std::size_t> completed{ 0 };
		std::atomic<std::size_t> overflow{ 0 };
		std::atomic<std::size_t> truncated{ 0 };
		std::atomic<std::size_t> warning_or_worse{ 0 };
		std::atomic<unsigned> calibrations{ 0 };
		std::atomic<unsigned> late_calibrations{ 0 };
		std::atomic<unsigned> destructions{ 0 };
		std::atomic<Phase> phase{ Phase::calibration };
		std::atomic<Phase> destruction_phase{ Phase::calibration };
	};

	// Microsoft permits callbacks on another thread and forbids D3D calls in
	// them: https://devblogs.microsoft.com/directx/d3d12-debug-layer-message-callback/
	// Each callback owns one slot. Payload never changes after ready publishes
	// it; the reporter reads it only after an acquire. No description pointer,
	// allocation, assertion, D3D call or exception escapes either callback.
	void CALLBACK capture_message(D3D12_MESSAGE_CATEGORY category,
		D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id,
		LPCSTR description, void* context) noexcept
	{
		Capture& capture = *static_cast<Capture*>(context);
		const std::size_t index = capture.started.fetch_add(1, std::memory_order_relaxed);
		if (severity <= D3D12_MESSAGE_SEVERITY_WARNING)
		{
			capture.warning_or_worse.fetch_add(1, std::memory_order_relaxed);
		}
		if (index >= message_capacity)
		{
			capture.overflow.fetch_add(1, std::memory_order_relaxed);
			capture.completed.fetch_add(1, std::memory_order_release);
			return;
		}
		Message& message = capture.messages[index];
		message.category = category;
		message.severity = severity;
		message.id = id;
		message.phase = capture.phase.load(std::memory_order_acquire);
		std::size_t length = 0;
		if (description != nullptr)
		{
			while (length + 1 < description_capacity && description[length] != '\0')
			{
				message.description[length] = description[length];
				++length;
			}
		}
		message.description[length] = '\0';
		if (description == nullptr || description[length] != '\0')
		{
			capture.truncated.fetch_add(1, std::memory_order_relaxed);
		}
		const bool calibration = severity == D3D12_MESSAGE_SEVERITY_INFO &&
			std::strcmp(message.description.data(), calibration_marker) == 0;
		const bool late_calibration = severity == D3D12_MESSAGE_SEVERITY_INFO &&
			std::strcmp(message.description.data(), teardown_marker) == 0;
		message.ready.store(true, std::memory_order_release);
		capture.completed.fetch_add(1, std::memory_order_release);
		if (calibration)
		{
			capture.calibrations.fetch_add(1, std::memory_order_release);
		}
		if (late_calibration)
		{
			capture.late_calibrations.fetch_add(1, std::memory_order_release);
		}
	}

	// This is an independent destruction witness, not a COM reference count.
	// The object being destroyed must not be accessed from this callback:
	// https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/nn-d3dcommon-id3ddestructionotifier
	void CALLBACK capture_destruction(void* context) noexcept
	{
		Capture& capture = *static_cast<Capture*>(context);
		capture.destruction_phase.store(capture.phase.load(std::memory_order_acquire),
			std::memory_order_relaxed);
		capture.destructions.fetch_add(1, std::memory_order_release);
	}

	class Watchdog
	{
	public:
		Watchdog() : deadline_(std::chrono::steady_clock::now() + std::chrono::seconds(10)),
			worker_([this]()
			{
				std::unique_lock<std::mutex> lock(this->mutex_);
				if (!this->condition_.wait_until(lock, this->deadline_,
					[this]() { return this->finished_; }))
				{
					std::fputs("D3D12 destruction probe: ten-second deadline expired; "
						"terminating only this probe process\n", stderr);
					std::fflush(stderr);
					std::_Exit(EXIT_FAILURE);
				}
			}) {}
		~Watchdog()
		{
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				this->check_deadline();
				this->finished_ = true;
			}
			this->condition_.notify_one();
			this->worker_.join();
			this->check_deadline();
		}
		Watchdog(const Watchdog&) = delete;
		Watchdog& operator=(const Watchdog&) = delete;
	private:
		void check_deadline() const noexcept
		{
			if (std::chrono::steady_clock::now() >= this->deadline_)
			{
				std::fputs("D3D12 destruction probe: ten-second completion deadline expired\n",
					stderr);
				std::fflush(stderr);
				std::_Exit(EXIT_FAILURE);
			}
		}
		std::mutex mutex_;
		std::condition_variable condition_;
		bool finished_ = false;
		const std::chrono::steady_clock::time_point deadline_;
		std::thread worker_;
	};

	void require(bool condition, const char* operation)
	{
		if (!condition)
		{
			throw std::runtime_error(operation);
		}
	}

	void require_hresult(HRESULT result, const char* operation)
	{
		std::printf("%s: HRESULT 0x%08lx\n", operation, static_cast<unsigned long>(result));
		std::fflush(stdout);
		require(SUCCEEDED(result), operation);
	}

	void await_marker(const std::atomic<unsigned>& counter, const char* operation)
	{
		const std::chrono::steady_clock::time_point deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (counter.load(std::memory_order_acquire) == 0 &&
			std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		require(counter.load(std::memory_order_acquire) == 1, operation);
	}

	void present_frame(Renderer& renderer, TextureHandle texture,
		std::array<unsigned, DeviceResources::frame_count>& visits, unsigned& mask)
	{
		renderer.begin_frame();
		const int slot = renderer.impl()->device_resources.frame_index();
		require(slot >= 0 && slot < DeviceResources::frame_count, "invalid swapchain frame index");
		++visits[static_cast<std::size_t>(slot)];
		mask |= 1u << static_cast<unsigned>(slot);
		renderer.set_view_count(1);
		renderer.view(0).draw_sprite(texture, RectangleI(0, 0, 2, 2),
			RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		renderer.submit();
		renderer.end_frame();
	}

	void workload(Capture*& capture)
	{
		render_tests::TestWindow window(initial_size, initial_size);
		RenderResources resources;
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
		renderer->create_device(window.handle(), initial_size, initial_size, 1);
		renderer->set_resources(&resources);
		{
			const RenderDeviceInfo& info = renderer->device_info();
			std::printf("backend=%s api=%s device=%s vendor=0x%04x device_id=0x%04x kind=%s\n",
				info.backend.c_str(), info.api.c_str(), info.device_name.c_str(),
				static_cast<unsigned>(info.vendor_id), static_cast<unsigned>(info.device_id),
				info.kind == RenderDeviceKind::hardware ? "hardware" : "software");
		}
		ComPtr<ID3D12InfoQueue1> messages;
		ComPtr<ID3DDestructionNotifier> notifier;
		{
			ID3D12Device* const borrowed = renderer->impl()->device_resources.device();
			require_hresult(borrowed->QueryInterface(IID_PPV_ARGS(messages.GetAddressOf())),
				"actual device QueryInterface(ID3D12InfoQueue1)");
			require_hresult(borrowed->QueryInterface(IID_PPV_ARGS(notifier.GetAddressOf())),
				"actual device QueryInterface(ID3DDestructionNotifier)");
		}
		// Capability checks precede this allocation and both registrations.
		// Intentionally process-lifetime (~70 KB): neither callback-quiescence
		// nor final destruction is assumed on a failed probe. It owns no COM data.
		capture = new Capture;
		DWORD message_cookie = 0;
		UINT destruction_cookie = 0;
		require_hresult(messages->RegisterMessageCallback(capture_message,
			D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, capture, &message_cookie),
			"RegisterMessageCallback(IGNORE_FILTERS)");
		require_hresult(notifier->RegisterDestructionCallback(capture_destruction,
			capture, &destruction_cookie), "RegisterDestructionCallback(actual device)");
		require_hresult(messages->AddApplicationMessage(D3D12_MESSAGE_SEVERITY_INFO,
			calibration_marker), "AddApplicationMessage(exact calibration)");
		await_marker(capture->calibrations,
			"exact INFO callback calibration was not observed once");
		// No filters or diagnostic COM interfaces span the workload/teardown.
		// The raw borrowed device above has already left scope; never reuse it.
		messages.Reset();
		notifier.Reset();
		capture->phase.store(Phase::frames, std::memory_order_release);
		TextureHandle texture;
		{
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(*renderer, resources, "destruction_probe", white);
			texture = resources.resolve_texture("destruction_probe");
		}
		std::array<unsigned, DeviceResources::frame_count> visits{};
		unsigned mask = 0;
		for (int frame = 0; frame < 3; ++frame)
		{
			present_frame(*renderer, texture, visits, mask);
		}
		capture->phase.store(Phase::resize, std::memory_order_release);
		require(window.resize(resized_size, resized_size), "real hidden-window resize failed");
		require(renderer->window_size_changed(resized_size, resized_size),
			"renderer resize failed");
		require(renderer->back_buffer_size() == Vector2F(
			static_cast<float>(resized_size), static_cast<float>(resized_size)),
			"resize dimensions differ");
		capture->phase.store(Phase::frames, std::memory_order_release);
		for (int frame = 0; frame < 3; ++frame)
		{
			present_frame(*renderer, texture, visits, mask);
		}
		std::printf("ordinary hidden presents=6 slot_mask=0x%x", mask);
		for (std::size_t slot = 0; slot < visits.size(); ++slot)
		{
			std::printf(" slot%zu=%u", slot, visits[slot]);
		}
		std::fputs(" (visits do not prove pending work or slot overlap)\n", stdout);
		capture->phase.store(Phase::before_teardown, std::memory_order_release);
		{
			ComPtr<ID3D12InfoQueue1> late_messages;
			require_hresult(renderer->impl()->device_resources.device()->QueryInterface(
				IID_PPV_ARGS(late_messages.GetAddressOf())),
				"late QueryInterface(ID3D12InfoQueue1)");
			// Use the original registration after a diagnostic-interface-free
			// workload interval. This temporary interface ends before teardown.
			require_hresult(late_messages->AddApplicationMessage(D3D12_MESSAGE_SEVERITY_INFO,
				teardown_marker), "AddApplicationMessage(exact late calibration)");
			await_marker(capture->late_calibrations,
				"exact late INFO callback calibration was not observed once");
		}
		capture->phase.store(Phase::renderer_release, std::memory_order_release);
		renderer.reset();
		capture->phase.store(Phase::texture_release, std::memory_order_release);
		resources.release_device_resources();
		capture->phase.store(Phase::owners_released, std::memory_order_release);
	}

	bool report(Capture& capture)
	{
		const std::chrono::steady_clock::time_point deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(5);
		std::size_t count = 0;
		bool settled = false;
		do
		{
			count = capture.started.load(std::memory_order_acquire);
			settled = capture.destructions.load(std::memory_order_acquire) == 1 &&
				capture.completed.load(std::memory_order_acquire) == count;
			for (std::size_t index = 0; settled && index < std::min(count, message_capacity);
				++index)
			{
				settled = capture.messages[index].ready.load(std::memory_order_acquire);
			}
			settled = settled && count == capture.started.load(std::memory_order_acquire);
			if (!settled)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		} while (!settled && std::chrono::steady_clock::now() < deadline);
		for (std::size_t index = 0; index < std::min(count, message_capacity); ++index)
		{
			const Message& message = capture.messages[index];
			if (message.ready.load(std::memory_order_acquire))
			{
				std::printf("message[%zu] phase=%s category=%d severity=%d id=%d: %s\n", index,
					phase_name(message.phase), static_cast<int>(message.category),
					static_cast<int>(message.severity), static_cast<int>(message.id),
					message.description.data());
			}
		}
		const std::size_t completed = capture.completed.load(std::memory_order_acquire);
		const std::size_t overflow = capture.overflow.load(std::memory_order_acquire);
		const std::size_t truncated = capture.truncated.load(std::memory_order_acquire);
		const std::size_t warnings = capture.warning_or_worse.load(std::memory_order_acquire);
		const unsigned calibrations = capture.calibrations.load(std::memory_order_acquire);
		const unsigned late_calibrations =
			capture.late_calibrations.load(std::memory_order_acquire);
		const unsigned destructions = capture.destructions.load(std::memory_order_acquire);
		std::printf("snapshot_started=%zu completed=%zu overflow=%zu truncated_or_null=%zu "
			"warning_or_worse=%zu calibrations=%u late_calibrations=%u destructions=%u "
			"destruction_phase=%s settled=%s\n",
			count, completed, overflow, truncated, warnings, calibrations, late_calibrations,
			destructions,
			phase_name(capture.destruction_phase.load(std::memory_order_acquire)),
			settled ? "yes" : "no");
		std::fputs("Observed callback snapshot only: initialization before registration "
			"and complete callback quiescence/coverage during destruction are not "
			"established.\n", stdout);
		return settled && completed == count && overflow == 0 && truncated == 0 && warnings == 0 &&
			calibrations == 1 && late_calibrations == 1 && destructions == 1;
	}
}
#endif

int main()
{
#ifdef NDEBUG
	std::fputs("D3D12DestructionProbe requires a Debug build with debug callbacks; "
		"Release is unverified and refused.\n", stderr);
	return EXIT_FAILURE;
#else
	Watchdog watchdog;
	Capture* capture = nullptr;
	bool workload_completed = false;
	try
	{
		workload(capture);
		workload_completed = true;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "D3D12DestructionProbe unverified: %s\n", error.what());
	}
	catch (...)
	{
		std::fputs("D3D12DestructionProbe unverified: unknown failure\n", stderr);
	}
	const bool captured = capture != nullptr && report(*capture);
	const bool passed = workload_completed && captured;
	std::printf("D3D12DestructionProbe: %s\n",
		passed ? "observations passed" : "FAILED / unverified");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

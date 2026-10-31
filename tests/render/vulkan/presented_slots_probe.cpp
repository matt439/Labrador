#include "tests/render/test_window.h"

#include "engine/render/vulkan/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
	using namespace labrador;

	constexpr int width = 1280;
	constexpr int height = 720;
	constexpr int unverified_exit = 2;
	constexpr uint64_t completion_timeout_ns = 5'000'000'000;
	constexpr std::array<int, 4> sprite_counts{ 1, 64, 256, 1024 };
	static_assert(DeviceResources::frame_count == 2);

	[[noreturn]] void fail_fast(const char* operation, VkResult result) noexcept
	{
		std::fprintf(stderr, "Vulkan presented-slots probe fatal: %s (VkResult %d)\n",
			operation, static_cast<int>(result));
		std::fflush(stderr);
		// A bounded wait failure must not initiate live-device recovery or
		// allow production to reset a command pool still used by the GPU.
		std::_Exit(EXIT_FAILURE);
	}

	class Watchdog
	{
	public:
		Watchdog() : deadline_(std::chrono::steady_clock::now() + std::chrono::seconds(10))
		{
			this->worker_ = std::thread([this]()
				{
					std::unique_lock<std::mutex> lock(this->mutex_);
					if (!this->condition_.wait_until(lock, this->deadline_,
						[this]() { return this->finished_; }))
					{
						fail_fast("ten-second process deadline expired", VK_TIMEOUT);
					}
				});
		}
		~Watchdog()
		{
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				if (std::chrono::steady_clock::now() >= this->deadline_)
				{
					fail_fast("process deadline expired before cancellation", VK_TIMEOUT);
				}
				this->finished_ = true;
			}
			this->condition_.notify_one();
			this->worker_.join();
			if (std::chrono::steady_clock::now() >= this->deadline_)
			{
				fail_fast("process deadline expired during watchdog join", VK_TIMEOUT);
			}
		}
		Watchdog(const Watchdog&) = delete;
		Watchdog& operator=(const Watchdog&) = delete;
	private:
		const std::chrono::steady_clock::time_point deadline_;
		std::mutex mutex_;
		std::condition_variable condition_;
		bool finished_ = false;
		std::thread worker_;
	};

	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
		{
			throw std::runtime_error(std::string("Vulkan loader export unavailable: ") + name);
		}
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}

	struct VulkanCalls
	{
		PFN_vkQueuePresentKHR present = loader_function<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");
		PFN_vkWaitSemaphores wait = loader_function<PFN_vkWaitSemaphores>("vkWaitSemaphores");
		PFN_vkGetSemaphoreCounterValue counter =
			loader_function<PFN_vkGetSemaphoreCounterValue>("vkGetSemaphoreCounterValue");
	};

	const VulkanCalls& real_calls()
	{
		static const VulkanCalls calls;
		return calls;
	}

	struct Observation
	{
		VkDevice device = VK_NULL_HANDLE;
		std::array<uint64_t, 2> targets{};
		uint64_t reuse_target = 0;
		unsigned calls = 0;
		uint64_t requested_timeout = 0;
		uint64_t counter_before = 0;
		bool zero_wait_ran = false;
		VkResult zero_wait = VK_NOT_READY;
		VkResult completion = VK_NOT_READY;
		bool pending = false;
	};
	thread_local Observation* active_observation = nullptr;

	struct Presentation
	{
		unsigned calls = 0;
		VkQueue queue = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkResult result = VK_NOT_READY;
	};
	thread_local Presentation* active_presentation = nullptr;

	class ObservePresentation
	{
	public:
		explicit ObservePresentation(Presentation& presentation) : previous_(active_presentation)
		{
			active_presentation = &presentation;
		}
		~ObservePresentation() { active_presentation = this->previous_; }
		ObservePresentation(const ObservePresentation&) = delete;
		ObservePresentation& operator=(const ObservePresentation&) = delete;
	private:
		Presentation* previous_;
	};

	class Observe
	{
	public:
		explicit Observe(Observation& observation) : previous_(active_observation)
		{
			active_observation = &observation;
		}
		~Observe() { active_observation = this->previous_; }
		Observe(const Observe&) = delete;
		Observe& operator=(const Observe&) = delete;
	private:
		Observation* previous_;
	};

	void draw_present(Renderer& renderer, TextureHandle texture, int sprites,
		Presentation* presentation = nullptr)
	{
		renderer.begin_frame();
		renderer.set_view_count(1);
		for (int sprite = 0; sprite < sprites; ++sprite)
		{
			renderer.view(0).draw_sprite(texture, RectangleI(0, 0, 2, 2),
				RectangleF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
				Colour::white, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
		}
		renderer.submit();
		if (presentation != nullptr)
		{
			ObservePresentation scope(*presentation);
			renderer.end_frame();
		}
		else
		{
			renderer.end_frame();
		}
	}

	int frame_index(const std::array<DeviceResources::Frame*, 2>& frames,
		const DeviceResources::Frame* frame)
	{
		if (frame == frames[0])
		{
			return 0;
		}
		if (frame == frames[1])
		{
			return 1;
		}
		throw std::runtime_error("frame address differs from both warmed slots");
	}

	int run_probe()
	{
		render_tests::TestWindow window(width, height);
		RenderResources resources;
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
		renderer->create_device(window.handle(), width, height, 1);
		renderer->set_resources(&resources);
		{
			const RenderDeviceInfo& info = renderer->device_info();
			std::printf("mode=observational backend=%s api=%s device=%s vendor=0x%04x "
				"device_id=0x%04x kind=%s build=%s fixture=%dx%d\n", info.backend.c_str(),
				info.api.c_str(), info.device_name.c_str(), static_cast<unsigned>(info.vendor_id),
				static_cast<unsigned>(info.device_id),
				info.kind == RenderDeviceKind::hardware ? "hardware" : "non-hardware",
#ifdef NDEBUG
				"Release",
#else
				"Debug",
#endif
				width, height);
			std::fflush(stdout);
			if (info.kind != RenderDeviceKind::hardware)
			{
				std::fputs("UNVERIFIED: this observation requires the actual hardware rasterizer\n",
					stderr);
				return unverified_exit;
			}
		}
		TextureHandle texture;
		{
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(*renderer, resources, "presented_slots_probe", white);
			texture = resources.resolve_texture("presented_slots_probe");
		}
		DeviceResources& device = renderer->impl()->device_resources;
		const VkDevice expected_device = device.device();
		const VkQueue expected_queue = device.owner().queue;
		// A new DeviceResources starts at index zero. The next ordinary
		// end_frame advances to one; these are borrowed identities, not owners.
		std::array<DeviceResources::Frame*, 2> frames{ &device.frame(), nullptr };
		draw_present(*renderer, texture, 1);
		frames[1] = &device.frame();
		if (frames[0] == frames[1])
		{
			throw std::runtime_error("warm presentation did not advance the frame slot");
		}
		draw_present(*renderer, texture, 1);
		device.wait_for_gpu();
		std::printf("warm presents=2 drained; frame0=%p frame1=%p\n",
			static_cast<void*>(frames[0]), static_cast<void*>(frames[1]));
		std::fflush(stdout);

		bool witnessed = false;
		for (std::size_t pair = 0; pair < sprite_counts.size(); ++pair)
		{
			device.wait_for_gpu();
			std::array<DeviceResources::Frame*, 2> submitted{};
			std::array<Presentation, 2> presentations{};
			for (std::size_t frame = 0; frame < submitted.size(); ++frame)
			{
				submitted[frame] = &device.frame();
				draw_present(*renderer, texture, sprite_counts[pair], &presentations[frame]);
			}
			if (device.device() != expected_device || submitted[0] == submitted[1])
			{
				throw std::runtime_error("device or distinct frame identities changed during the "
					"pair");
			}
			// end_frame can advance without a swapchain. Only two exact API
			// observations on the same valid queue/chain qualify as presented.
			// SUBOPTIMAL is an actual successful present, reported explicitly.
			const bool actual_presents = presentations[0].calls == 1 &&
				presentations[1].calls == 1 &&
				presentations[0].queue == expected_queue &&
				presentations[1].queue == expected_queue &&
				presentations[0].swapchain != VK_NULL_HANDLE &&
				presentations[0].swapchain == presentations[1].swapchain &&
				(presentations[0].result == VK_SUCCESS ||
					presentations[0].result == VK_SUBOPTIMAL_KHR) &&
				(presentations[1].result == VK_SUCCESS ||
					presentations[1].result == VK_SUBOPTIMAL_KHR);
			if (!actual_presents)
			{
				std::printf("pair=%zu Present calls=%u,%u queues=%p,%p swapchains=%p,%p "
					"results=%d,%d decision=UNVERIFIED-presented-pair\n", pair,
					presentations[0].calls, presentations[1].calls,
					reinterpret_cast<void*>(presentations[0].queue),
					reinterpret_cast<void*>(presentations[1].queue),
					reinterpret_cast<void*>(presentations[0].swapchain),
					reinterpret_cast<void*>(presentations[1].swapchain),
					static_cast<int>(presentations[0].result),
					static_cast<int>(presentations[1].result));
				for (const Presentation& presentation : presentations)
				{
					// OUT_OF_DATE is rebuilt by the backend and leaves this pair
					// unverified. Surface loss and other API errors remain failures,
					// including when check_vk has already thrown out of end_frame.
					if (presentation.result < 0 && presentation.result != VK_ERROR_OUT_OF_DATE_KHR)
					{
						throw std::runtime_error("actual Present returned an API failure");
					}
				}
				break;
			}
			Observation observation;
			observation.device = expected_device;
			// Read both fresh targets AFTER both actual end_frame calls. Avoid
			// logging/allocation between the pair and the exact reuse observer.
			observation.targets = { submitted[0]->timeline_value, submitted[1]->timeline_value };
			observation.reuse_target = device.frame().timeline_value;
			if (observation.targets[0] == 0 || observation.targets[1] <= observation.targets[0] ||
				(observation.reuse_target != observation.targets[0] &&
					observation.reuse_target != observation.targets[1]))
			{
				throw std::runtime_error("fresh pair timeline targets are inconsistent");
			}
			{
				Observe scope(observation);
				renderer->begin_frame();
			}
			witnessed = observation.calls == 1 && observation.pending &&
				observation.completion == VK_SUCCESS;
			std::printf("pair=%zu sprites_per_frame=%d slots=%d,%d frame_addresses=%p,%p "
				"targets=%llu,%llu reuse_target=%llu observer_calls=%u counter=%llu "
				"zero_wait_ran=%s zero_wait=%d completion=%d requested_timeout=%llu "
				"Present_calls=%u,%u Present_results=%d,%d decision=%s\n", pair,
				sprite_counts[pair], frame_index(frames, submitted[0]),
				frame_index(frames, submitted[1]),
				static_cast<void*>(submitted[0]), static_cast<void*>(submitted[1]),
				static_cast<unsigned long long>(observation.targets[0]),
				static_cast<unsigned long long>(observation.targets[1]),
				static_cast<unsigned long long>(observation.reuse_target), observation.calls,
				static_cast<unsigned long long>(observation.counter_before),
				observation.zero_wait_ran ? "yes" : "no",
				static_cast<int>(observation.zero_wait), static_cast<int>(observation.completion),
				static_cast<unsigned long long>(observation.requested_timeout),
				presentations[0].calls, presentations[1].calls,
				static_cast<int>(presentations[0].result),
				static_cast<int>(presentations[1].result),
				witnessed ? "pending-instant-observed" : "UNVERIFIED");
			std::fflush(stdout);
			if (witnessed)
			{
				break;
			}
		}
		device.wait_for_gpu();
		renderer.reset();
		resources.release_device_resources();
		return witnessed ? EXIT_SUCCESS : unverified_exit;
	}
}

// Observe only an end_frame scope. Every call forwards the original queue,
// structure pointer and result; no presentation is synthesized or deferred.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(
	VkQueue queue, const VkPresentInfoKHR* presentation)
{
	const VulkanCalls& calls = real_calls();
	Presentation* const record = active_presentation;
	if (record == nullptr)
	{
		return calls.present(queue, presentation);
	}
	if (queue == VK_NULL_HANDLE || presentation == nullptr ||
		presentation->sType != VK_STRUCTURE_TYPE_PRESENT_INFO_KHR ||
		presentation->swapchainCount != 1 ||
		presentation->pSwapchains == nullptr || presentation->pSwapchains[0] == VK_NULL_HANDLE)
	{
		fail_fast("unexpected actual Present shape", VK_ERROR_UNKNOWN);
	}
	++record->calls;
	record->queue = queue;
	record->swapchain = presentation->pSwapchains[0];
	const VkResult result = calls.present(queue, presentation);
	record->result = result;
	return result;
}

// No artificial holds: a pending host-set event would violate
// VUID-vkSetEvent-event-09543, and unresolved semaphore signals cannot precede
// actual binary Present waits (VUID-vkQueuePresentKHR-pWaitSemaphores-03268).
// Only the exact scoped reuse wait is observed/bounded. Every unarmed call
// forwards its original device, structure pointer and timeout unchanged.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphores(
	VkDevice device, const VkSemaphoreWaitInfo* wait, uint64_t timeout)
{
	const VulkanCalls& calls = real_calls();
	Observation* const observation = active_observation;
	if (observation == nullptr)
	{
		return calls.wait(device, wait, timeout);
	}
	++observation->calls;
	if (observation->calls != 1 || device != observation->device || wait == nullptr ||
		wait->sType != VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO || wait->pNext != nullptr ||
		wait->flags != 0 || wait->semaphoreCount != 1 || wait->pSemaphores == nullptr ||
		wait->pSemaphores[0] == VK_NULL_HANDLE ||
		wait->pValues == nullptr || wait->pValues[0] != observation->reuse_target ||
		timeout != UINT64_MAX)
	{
		fail_fast("unexpected actual reuse-wait shape", VK_ERROR_UNKNOWN);
	}
	observation->requested_timeout = timeout;
	VkResult result = calls.counter(device, wait->pSemaphores[0], &observation->counter_before);
	if (result != VK_SUCCESS)
	{
		fail_fast("actual timeline counter query failed", result);
	}
	const uint64_t minimum_target = std::min(observation->targets[0], observation->targets[1]);
	if (observation->counter_before < observation->targets[0] &&
		observation->counter_before < observation->targets[1])
	{
		VkSemaphoreWaitInfo zero_wait = *wait;
		zero_wait.pValues = &minimum_target;
		observation->zero_wait_ran = true;
		observation->zero_wait = calls.wait(device, &zero_wait, 0);
		if (observation->zero_wait != VK_SUCCESS && observation->zero_wait != VK_TIMEOUT)
		{
			fail_fast("zero-time shared timeline oracle failed", observation->zero_wait);
		}
		// A timeout on the smaller value witnesses BOTH outstanding targets
		// on this one production timeline at that instant. A healthy completed
		// pair or a race to completion is unverified, never a passing smoke.
		observation->pending = observation->zero_wait == VK_TIMEOUT;
	}
	result = calls.wait(device, wait, std::min(timeout, completion_timeout_ns));
	if (result != VK_SUCCESS)
	{
		fail_fast("bounded actual reuse wait failed", result);
	}
	observation->completion = result;
	return result;
}

int main()
{
	// Arm the absolute deadline before its worker starts; it outlives every
	// renderer/resource owner, including exception cleanup.
	Watchdog watchdog;
	try
	{
		static_cast<void>(real_calls()); // Resolve actual exports before any engine Vulkan call.
		const int result = run_probe();
		std::fputs(result == EXIT_SUCCESS
			? "OBSERVED: two ordinary presented frame slots were unretired at the "
				"actual reuse wait.\n"
			: "UNVERIFIED: no two-slot pending instant witnessed in the bounded "
				"ordinary pairs.\n",
			stdout);
		std::fputs("Observational ordering only; no deterministic pending generation, "
			"GPU saturation, physical display overlap, final-device or complete "
			"layer-diagnostic claim.\n", stdout);
		return result;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "FAILED: %s\n", error.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::fputs("FAILED: unknown exception\n", stderr);
		return EXIT_FAILURE;
	}
}

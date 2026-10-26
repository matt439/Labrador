#include "tests/render/test_window.h"

#include "engine/render/vulkan/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#ifndef NDEBUG
namespace
{
	using namespace labrador;
	using namespace mattmath;

	constexpr std::size_t message_capacity = 2048;
	constexpr std::size_t description_capacity = 512;
	constexpr int initial_size = 64;
	constexpr int resized_size = 32;
	constexpr char first_marker[] = "Labrador Vulkan device-destruction callback calibration";
	constexpr char last_marker[] = "Labrador Vulkan callback before device teardown";

	enum class Phase
	{
		setup,
		frames,
		resize,
		before_teardown,
		renderer_release,
		texture_release,
		device_destroy,
		instance_destroy,
		owners_released,
	};

	const char* phase_name(Phase phase)
	{
		switch (phase)
		{
		case Phase::setup: return "setup";
		case Phase::frames: return "frames";
		case Phase::resize: return "resize";
		case Phase::before_teardown: return "before-teardown";
		case Phase::renderer_release: return "renderer-release";
		case Phase::texture_release: return "texture-release";
		case Phase::device_destroy: return "device-destroy";
		case Phase::instance_destroy: return "instance-destroy";
		case Phase::owners_released: return "owners-released";
		}
		return "unknown";
	}

	struct VulkanCalls
	{
		PFN_vkDestroyDevice destroy_device = nullptr;
		PFN_vkDestroyInstance destroy_instance = nullptr;
	};
	VulkanCalls actual;

	struct Message
	{
		std::atomic<bool> ready{ false };
		VkDebugUtilsMessageSeverityFlagBitsEXT severity{};
		VkDebugUtilsMessageTypeFlagsEXT type = 0;
		std::int32_t id = 0;
		Phase phase = Phase::setup;
		std::array<char, description_capacity> description{};
	};

	struct CallbackState
	{
		// Raw identities and instance functions, not a shared VulkanDevice owner.
		VkInstance instance = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		PFN_vkCreateDebugUtilsMessengerEXT create_messenger = nullptr;
		PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger = nullptr;
		PFN_vkSubmitDebugUtilsMessageEXT submit_message = nullptr;
		std::atomic<VkDebugUtilsMessengerEXT> messenger{ VK_NULL_HANDLE };
		std::array<Message, message_capacity> messages;
		std::atomic<std::size_t> started{ 0 };
		std::atomic<std::size_t> completed{ 0 };
		std::atomic<std::size_t> overflow{ 0 };
		std::atomic<std::size_t> truncated_or_null{ 0 };
		std::atomic<std::size_t> errors{ 0 };
		std::atomic<std::size_t> validation_warnings{ 0 };
		std::atomic<std::size_t> performance_advisories{ 0 };
		std::atomic<unsigned> first_calibrations{ 0 };
		std::atomic<unsigned> last_calibrations{ 0 };
		std::atomic<unsigned> device_entered{ 0 };
		std::atomic<unsigned> device_returned{ 0 };
		std::atomic<unsigned> instance_entered{ 0 };
		std::atomic<unsigned> instance_returned{ 0 };
		std::atomic<unsigned> messenger_destroyed{ 0 };
		std::atomic<Phase> phase{ Phase::setup };
		std::atomic<Phase> device_release_phase{ Phase::setup };
	};
	std::atomic<CallbackState*> armed{ nullptr };

	[[noreturn]] void fail_fast(const char* message) noexcept
	{
		std::fprintf(stderr, "Vulkan destruction probe fatal: %s\n", message);
		std::fflush(stderr);
		std::_Exit(EXIT_FAILURE);
	}

	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
			throw std::runtime_error(std::string("Vulkan loader export unavailable: ") + name);
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}

	template<typename Function>
	Function instance_function(VkInstance instance, const char* name)
	{
		const PFN_vkVoidFunction address = vkGetInstanceProcAddr(instance, name);
		if (address == nullptr)
			throw std::runtime_error(std::string("Actual instance function unavailable: ") + name);
		static_assert(sizeof(Function) == sizeof(PFN_vkVoidFunction));
		return std::bit_cast<Function>(address);
	}

	// VUID-PFN_vkDebugUtilsMessengerCallbackEXT-None-04769 forbids Vulkan
	// commands here. Payload is copied before a release publication; neither
	// callback data nor its strings are retained. Storage lasts until process exit.
	VKAPI_ATTR VkBool32 VKAPI_CALL capture_message(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* data, void* context) noexcept
	{
		CallbackState& capture = *static_cast<CallbackState*>(context);
		const std::size_t index = capture.started.fetch_add(1, std::memory_order_relaxed);
		if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			capture.errors.fetch_add(1, std::memory_order_relaxed);
		if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
				capture.performance_advisories.fetch_add(1, std::memory_order_relaxed);
			else
				capture.validation_warnings.fetch_add(1, std::memory_order_relaxed);
		}
		if (index >= message_capacity)
		{
			capture.overflow.fetch_add(1, std::memory_order_relaxed);
			capture.completed.fetch_add(1, std::memory_order_release);
			return VK_FALSE;
		}
		Message& message = capture.messages[index];
		message.severity = severity;
		message.type = type;
		message.id = data == nullptr ? 0 : data->messageIdNumber;
		message.phase = capture.phase.load(std::memory_order_acquire);
		const char* description = data == nullptr ? nullptr : data->pMessage;
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
			capture.truncated_or_null.fetch_add(1, std::memory_order_relaxed);
		const bool first = severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT &&
			std::strcmp(message.description.data(), first_marker) == 0;
		const bool last = severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT &&
			std::strcmp(message.description.data(), last_marker) == 0;
		message.ready.store(true, std::memory_order_release);
		capture.completed.fetch_add(1, std::memory_order_release);
		if (first) capture.first_calibrations.fetch_add(1, std::memory_order_release);
		if (last) capture.last_calibrations.fetch_add(1, std::memory_order_release);
		return VK_FALSE;
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
					fail_fast("ten-second process deadline expired");
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
				fail_fast("ten-second completion deadline expired");
			}
		}
		std::mutex mutex_;
		std::condition_variable condition_;
		bool finished_ = false;
		const std::chrono::steady_clock::time_point deadline_;
		std::thread worker_;
	};

	void require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void marker(CallbackState& capture, const char* text, std::atomic<unsigned>& counter)
	{
		VkDebugUtilsMessengerCallbackDataEXT data = {};
		data.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
		data.pMessageIdName = "LabradorDestructionProbe";
		data.pMessage = text;
		capture.submit_message(capture.instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &data);
		const std::chrono::steady_clock::time_point deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (counter.load(std::memory_order_acquire) == 0 &&
			std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		require(counter.load(std::memory_order_acquire) == 1,
			"exact INFO calibration callback was not observed once");
	}

	void present_frame(Renderer& renderer, TextureHandle texture)
	{
		renderer.begin_frame();
		renderer.set_view_count(1);
		renderer.view(0).draw_sprite(texture, RectangleI(0, 0, 2, 2),
			RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::white, 0.0f,
			Vector2F::ZERO, SpriteFlip::none, 0.0f);
		renderer.submit();
		renderer.end_frame();
	}

	void workload(CallbackState*& capture)
	{
		// Resolve unarmed forwarding before the engine can create/destroy anything.
		actual.destroy_device = loader_function<PFN_vkDestroyDevice>("vkDestroyDevice");
		actual.destroy_instance = loader_function<PFN_vkDestroyInstance>("vkDestroyInstance");
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
		{
			const VulkanDevice& owner = renderer->impl()->device_resources.owner();
			// The current backend creates this only when it enabled validation
			// and VK_EXT_debug_utils. A nonnull PFN alone cannot establish that.
			require(owner.messenger != VK_NULL_HANDLE,
				"actual Debug engine validation messenger is unavailable");
			capture = new CallbackState;
			capture->instance = owner.instance;
			capture->device = owner.device;
			capture->create_messenger = instance_function<PFN_vkCreateDebugUtilsMessengerEXT>(
				owner.instance, "vkCreateDebugUtilsMessengerEXT");
			capture->destroy_messenger = instance_function<PFN_vkDestroyDebugUtilsMessengerEXT>(
				owner.instance, "vkDestroyDebugUtilsMessengerEXT");
			capture->submit_message = instance_function<PFN_vkSubmitDebugUtilsMessageEXT>(
				owner.instance, "vkSubmitDebugUtilsMessageEXT");
		}
		// Process-lifetime fixed storage owns no device reference. Publish all
		// dispatch/identity fields before a failure can unwind a test messenger.
		armed.store(capture, std::memory_order_release);
		VkDebugUtilsMessengerCreateInfoEXT description = {};
		description.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		description.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		description.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		description.pfnUserCallback = capture_message;
		description.pUserData = capture;
		VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
		const VkResult created = capture->create_messenger(capture->instance, &description,
			nullptr, &messenger);
		if (created == VK_SUCCESS) capture->messenger.store(messenger, std::memory_order_release);
		std::printf("actual instance create test messenger: %s (%d)\n",
			vk_result_name(created).c_str(), static_cast<int>(created));
		std::fflush(stdout);
		check_vk(created, "The test debug-utils messenger could not be created");
		require(messenger != VK_NULL_HANDLE, "successful test messenger creation returned null");
		marker(*capture, first_marker, capture->first_calibrations);

		TextureHandle texture;
		{
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(*renderer, resources, "vulkan_destruction_probe", white);
			texture = resources.resolve_texture("vulkan_destruction_probe");
		}
		capture->phase.store(Phase::frames, std::memory_order_release);
		for (int frame = 0; frame < 3; ++frame)
		{
			present_frame(*renderer, texture);
		}
		capture->phase.store(Phase::resize, std::memory_order_release);
		require(window.resize(resized_size, resized_size), "real hidden-window resize failed");
		require(renderer->window_size_changed(resized_size, resized_size), "renderer resize failed");
		require(renderer->back_buffer_size() == Vector2F(
			static_cast<float>(resized_size), static_cast<float>(resized_size)), "resize dimensions differ");
		capture->phase.store(Phase::frames, std::memory_order_release);
		for (int frame = 0; frame < 3; ++frame)
		{
			present_frame(*renderer, texture);
		}
		{
			const VulkanDevice& owner = renderer->impl()->device_resources.owner();
			require(owner.instance == capture->instance && owner.device == capture->device &&
				capture->device_entered.load(std::memory_order_acquire) == 0 &&
				capture->instance_entered.load(std::memory_order_acquire) == 0,
				"the observed device or instance changed during the workload");
		}
		std::puts("ordinary hidden presents=6 real resize=64-to-32; no read-back, gates or overlap proof");
		capture->phase.store(Phase::before_teardown, std::memory_order_release);
		marker(*capture, last_marker, capture->last_calibrations);
		capture->phase.store(Phase::renderer_release, std::memory_order_release);
		renderer.reset();
		capture->phase.store(Phase::texture_release, std::memory_order_release);
		resources.release_device_resources();
		capture->phase.store(Phase::owners_released, std::memory_order_release);
	}

	bool report(CallbackState& capture)
	{
		const std::chrono::steady_clock::time_point deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(2);
		std::size_t count = 0;
		bool settled = false;
		do
		{
			count = capture.started.load(std::memory_order_acquire);
			settled = capture.device_returned.load(std::memory_order_acquire) == 1 &&
				capture.instance_returned.load(std::memory_order_acquire) == 1 &&
				capture.completed.load(std::memory_order_acquire) == count;
			for (std::size_t index = 0; settled && index < std::min(count, message_capacity); ++index)
				settled = capture.messages[index].ready.load(std::memory_order_acquire);
			settled = settled && count == capture.started.load(std::memory_order_acquire);
			if (!settled) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} while (!settled && std::chrono::steady_clock::now() < deadline);
		for (std::size_t index = 0; index < std::min(count, message_capacity); ++index)
		{
			const Message& message = capture.messages[index];
			if (message.ready.load(std::memory_order_acquire))
				std::printf("message[%zu] phase=%s severity=0x%x type=0x%x id=%d: %s\n",
					index, phase_name(message.phase), static_cast<unsigned>(message.severity),
					static_cast<unsigned>(message.type), message.id, message.description.data());
		}
		const std::size_t completed = capture.completed.load(std::memory_order_acquire);
		const std::size_t overflow = capture.overflow.load(std::memory_order_acquire);
		const std::size_t truncated = capture.truncated_or_null.load(std::memory_order_acquire);
		const std::size_t errors = capture.errors.load(std::memory_order_acquire);
		const std::size_t warnings = capture.validation_warnings.load(std::memory_order_acquire);
		std::printf("snapshot_started=%zu completed=%zu overflow=%zu truncated_or_null=%zu "
			"errors=%zu validation_or_general_warnings=%zu performance_only_advisories=%zu "
			"first_controls=%u late_controls=%u device_entered=%u device_returned=%u "
			"test_messenger_destroyed=%u instance_entered=%u instance_returned=%u "
			"device_release_phase=%s settled=%s\n", count, completed, overflow, truncated,
			errors, warnings, capture.performance_advisories.load(std::memory_order_acquire),
			capture.first_calibrations.load(std::memory_order_acquire),
			capture.last_calibrations.load(std::memory_order_acquire),
			capture.device_entered.load(std::memory_order_acquire),
			capture.device_returned.load(std::memory_order_acquire),
			capture.messenger_destroyed.load(std::memory_order_acquire),
			capture.instance_entered.load(std::memory_order_acquire),
			capture.instance_returned.load(std::memory_order_acquire),
			phase_name(capture.device_release_phase.load(std::memory_order_acquire)),
			settled ? "yes" : "no");
		std::puts("Observed callback snapshot and forwarded device-destroy entry/return only: "
			"initialization, actual instance-destroy diagnostics, complete callback quiescence "
			"and presented-slot overlap are not established.");
		return settled && completed == count && overflow == 0 && truncated == 0 &&
			errors == 0 && warnings == 0 &&
			capture.first_calibrations.load(std::memory_order_acquire) == 1 &&
			capture.last_calibrations.load(std::memory_order_acquire) == 1 &&
			capture.device_entered.load(std::memory_order_acquire) == 1 &&
			capture.device_returned.load(std::memory_order_acquire) == 1 &&
			capture.messenger_destroyed.load(std::memory_order_acquire) == 1 &&
			capture.instance_entered.load(std::memory_order_acquire) == 1 &&
			capture.instance_returned.load(std::memory_order_acquire) == 1;
	}
}

// Match only the captured identities. Unrelated calls keep their arguments,
// and every forward uses the actual DLL export rather than this executable.
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
	VkDevice device, const VkAllocationCallbacks* allocator)
{
	if (actual.destroy_device == nullptr) fail_fast("device destroy export was not pre-resolved");
	CallbackState* const capture = armed.load(std::memory_order_acquire);
	if (capture == nullptr || device != capture->device)
	{
		actual.destroy_device(device, allocator);
		return;
	}
	const Phase previous = capture->phase.exchange(Phase::device_destroy, std::memory_order_acq_rel);
	capture->device_release_phase.store(previous, std::memory_order_relaxed);
	capture->device_entered.fetch_add(1, std::memory_order_release);
	actual.destroy_device(device, allocator);
	capture->phase.store(previous, std::memory_order_release);
	capture->device_returned.fetch_add(1, std::memory_order_release);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
	VkInstance instance, const VkAllocationCallbacks* allocator)
{
	if (actual.destroy_instance == nullptr) fail_fast("instance destroy export was not pre-resolved");
	CallbackState* const capture = armed.load(std::memory_order_acquire);
	if (capture == nullptr || instance != capture->instance)
	{
		actual.destroy_instance(instance, allocator);
		return;
	}
	const Phase previous = capture->phase.exchange(Phase::instance_destroy, std::memory_order_acq_rel);
	capture->instance_entered.fetch_add(1, std::memory_order_release);
	// VUID-vkDestroyInstance-instance-00629: remove the test's instance child
	// first. Its creation used NULL, so 01916 requires NULL here too. Leave
	// the engine's messenger and both original allocator arguments alone.
	const VkDebugUtilsMessengerEXT messenger = capture->messenger.exchange(
		VK_NULL_HANDLE, std::memory_order_acq_rel);
	if (messenger != VK_NULL_HANDLE)
	{
		capture->destroy_messenger(instance, messenger, nullptr);
		capture->messenger_destroyed.fetch_add(1, std::memory_order_release);
	}
	actual.destroy_instance(instance, allocator);
	capture->phase.store(previous, std::memory_order_release);
	capture->instance_returned.fetch_add(1, std::memory_order_release);
}
#endif

int main()
{
#ifdef NDEBUG
	std::fputs("VulkanDestructionProbe requires Debug validation; Release is unverified and refused.\n", stderr);
	return EXIT_FAILURE;
#else
	Watchdog watchdog;
	CallbackState* capture = nullptr;
	bool workload_completed = false;
	try
	{
		workload(capture);
		workload_completed = true;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "VulkanDestructionProbe unverified: %s\n", error.what());
	}
	catch (...)
	{
		std::fputs("VulkanDestructionProbe unverified: unknown failure\n", stderr);
	}
	const bool captured = capture != nullptr && report(*capture);
	const bool passed = workload_completed && captured;
	std::printf("VulkanDestructionProbe: %s\n", passed ? "observations passed" : "FAILED / unverified");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

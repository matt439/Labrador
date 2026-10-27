#include <doctest/doctest.h>
#include "tests/render/test_window.h"

#include "engine/render/vulkan/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
	using namespace labrador;
	using namespace labrador;

	constexpr uint64_t completion_timeout_ns = 5'000'000'000;
	constexpr int initial_size = 64;
	constexpr int resized_size = 32;

	[[noreturn]] void fail_fast(const char* operation, VkResult result,
		uint64_t observed = 0) noexcept
	{
		std::fprintf(stderr, "Vulkan pending-submission test fatal: %s "
			"(VkResult %d, observed %llu)\n", operation, static_cast<int>(result),
			static_cast<unsigned long long>(observed));
		std::fflush(stderr);
		// Never return a timeout into try_wait_for_gpu: false can initiate
		// live-device recovery or let a destructor release pending resources.
		// Terminate only this test executable, without forcing device loss.
		std::_Exit(EXIT_FAILURE);
	}

	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
		{
			throw std::runtime_error(std::string("Vulkan test export missing: ") + name);
		}
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}

	struct VulkanCalls
	{
		PFN_vkQueueSubmit submit = loader_function<PFN_vkQueueSubmit>("vkQueueSubmit");
		PFN_vkWaitSemaphores wait = loader_function<PFN_vkWaitSemaphores>("vkWaitSemaphores");
		PFN_vkDeviceWaitIdle idle = loader_function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle");
		PFN_vkSignalSemaphore signal = loader_function<PFN_vkSignalSemaphore>("vkSignalSemaphore");
		PFN_vkGetSemaphoreCounterValue counter =
			loader_function<PFN_vkGetSemaphoreCounterValue>("vkGetSemaphoreCounterValue");
	};

	const VulkanCalls& real_calls()
	{
		static const VulkanCalls calls;
		return calls;
	}

	class Semaphore
	{
	public:
		explicit Semaphore(VkDevice device) : device_(device)
		{
			VkSemaphoreTypeCreateInfo type = {};
			type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
			type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
			VkSemaphoreCreateInfo description = {};
			description.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			description.pNext = &type;
			check_vk(vkCreateSemaphore(device, &description, nullptr, &this->semaphore_),
				"The pending-submission test semaphore could not be created");
		}
		~Semaphore() { vkDestroySemaphore(this->device_, this->semaphore_, nullptr); }
		Semaphore(const Semaphore&) = delete;
		Semaphore& operator=(const Semaphore&) = delete;
		VkSemaphore handle() const { return this->semaphore_; }
	private:
		VkDevice device_;
		VkSemaphore semaphore_ = VK_NULL_HANDLE;
	};

	class Fence
	{
	public:
		explicit Fence(VkDevice device) : device_(device)
		{
			VkFenceCreateInfo description = {};
			description.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			check_vk(vkCreateFence(device, &description, nullptr, &this->fence_),
				"The pending-submission test tail fence could not be created");
		}
		~Fence() { vkDestroyFence(this->device_, this->fence_, nullptr); }
		Fence(const Fence&) = delete;
		Fence& operator=(const Fence&) = delete;
		VkFence handle() const { return this->fence_; }
	private:
		VkDevice device_;
		VkFence fence_ = VK_NULL_HANDLE;
	};

	class QueueGate
	{
	public:
		explicit QueueGate(DeviceResources& resources) :
			owner_(resources.shared_device()), gate_(this->owner_->device),
			witness_(this->owner_->device),
			tail_(this->owner_->device)
		{
			// Resolve before arming. Only the worker ever signals gate value 1:
			// repeating a timeline signal is not the D3D12 idempotent operation.
			static_cast<void>(real_calls());
			this->deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			this->worker_ = std::thread([this]()
				{
					std::unique_lock<std::mutex> lock(this->mutex_);
					const bool requested = this->condition_.wait_until(lock, this->deadline_,
						[this]() { return this->release_requested_; });
					++this->signal_calls_;
					lock.unlock();
					VkSemaphoreSignalInfo signal = {};
					signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
					signal.semaphore = this->gate_.handle();
					signal.value = 1;
					const VkResult result = real_calls().signal(this->device(), &signal);
					if (result != VK_SUCCESS)
					{
						fail_fast("sole CPU gate signal failed", result);
					}
					lock.lock();
					this->signal_result_ = result;
					this->released_ = true;
					this->condition_.notify_all();
					if (!requested)
					{
						lock.unlock();
						fail_fast("release watchdog expired", VK_TIMEOUT);
					}
					// Keep the same absolute deadline through boundary completion
					// and tail retirement. No second signal occurs on this timeout.
					if (!this->condition_.wait_until(lock, this->deadline_,
						[this]() { return this->retired_; }))
					{
						lock.unlock();
						fail_fast("boundary/retirement watchdog expired", VK_TIMEOUT);
					}
				});
		}

		~QueueGate() { this->finish(); }
		QueueGate(const QueueGate&) = delete;
		QueueGate& operator=(const QueueGate&) = delete;

		VkDevice device() const { return this->owner_->device; }
		VkQueue queue() const { return this->owner_->queue; }
		VkSemaphore witness_semaphore() const { return this->witness_.handle(); }

		uint64_t witness() const noexcept
		{
			uint64_t value = 0;
			const VkResult result = real_calls().counter(this->device(), this->witness_.handle(), &value);
			if (result != VK_SUCCESS)
			{
				fail_fast("witness counter query failed", result);
			}
			return value;
		}

		void block_queue()
		{
			const VkSemaphore gate = this->gate_.handle();
			const VkSemaphore witness = this->witness_.handle();
			const uint64_t one = 1;
			const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			VkTimelineSemaphoreSubmitInfo timeline = {};
			timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
			timeline.waitSemaphoreValueCount = 1;
			timeline.pWaitSemaphoreValues = &one;
			timeline.signalSemaphoreValueCount = 1;
			timeline.pSignalSemaphoreValues = &one;
			VkSubmitInfo submission = {};
			submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submission.pNext = &timeline;
			submission.waitSemaphoreCount = 1;
			submission.pWaitSemaphores = &gate;
			submission.pWaitDstStageMask = &stage;
			submission.signalSemaphoreCount = 1;
			submission.pSignalSemaphores = &witness;
			check_vk(vkQueueSubmit(this->owner_->queue, 1, &submission, VK_NULL_HANDLE),
				"The pending-submission timeline gate batch could not be submitted");
		}

		void release_and_wait() noexcept
		{
			std::unique_lock<std::mutex> lock(this->mutex_);
			this->release_requested_ = true;
			this->condition_.notify_all();
			const std::chrono::steady_clock::time_point acknowledgement_deadline =
				std::min(this->deadline_, std::chrono::steady_clock::now() + std::chrono::seconds(5));
			if (!this->condition_.wait_until(lock, acknowledgement_deadline,
				[this]() { return this->released_; }))
			{
				lock.unlock();
				fail_fast("CPU gate signal acknowledgement expired", VK_TIMEOUT);
			}
		}

		[[noreturn]] void abort(const char* operation, VkResult result,
			uint64_t observed = 0) noexcept
		{
			this->release_and_wait();
			fail_fast(operation, result, observed);
		}

		void finish() noexcept
		{
			if (this->finished_)
			{
				return;
			}
			this->release_and_wait();
			VkSubmitInfo submission = {};
			submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			VkResult result = vkQueueSubmit(this->owner_->queue, 1, &submission, this->tail_.handle());
			if (result != VK_SUCCESS)
			{
				fail_fast("post-boundary tail submission failed", result);
			}
			const VkFence fence = this->tail_.handle();
			result = vkWaitForFences(this->device(), 1, &fence, VK_TRUE, completion_timeout_ns);
			if (result != VK_SUCCESS)
			{
				fail_fast("post-boundary tail wait failed", result);
			}
			{
				std::lock_guard<std::mutex> lock(this->mutex_);
				this->retired_ = true;
			}
			this->condition_.notify_all();
			this->worker_.join();
			this->finished_ = true;
		}

		// Read only after finish joined the sole writer.
		int signal_calls() const { return this->signal_calls_; }
		VkResult signal_result() const { return this->signal_result_; }

	private:
		std::shared_ptr<VulkanDevice> owner_;
		Semaphore gate_;
		Semaphore witness_;
		Fence tail_;
		std::chrono::steady_clock::time_point deadline_;
		std::mutex mutex_;
		std::condition_variable condition_;
		bool release_requested_ = false;
		bool released_ = false;
		bool retired_ = false;
		bool finished_ = false;
		int signal_calls_ = 0;
		VkResult signal_result_ = VK_NOT_READY;
		std::thread worker_;
	};

	struct SubmitRecord
	{
		QueueGate* gate = nullptr;
		VkCommandBuffer expected_commands = VK_NULL_HANDLE;
		int calls = 0;
		int injected = 0;
	};

	thread_local SubmitRecord* active_submit = nullptr;

	class InjectSubmit
	{
	public:
		explicit InjectSubmit(SubmitRecord& record) : previous_(active_submit) { active_submit = &record; }
		~InjectSubmit() { active_submit = this->previous_; }
		InjectSubmit(const InjectSubmit&) = delete;
		InjectSubmit& operator=(const InjectSubmit&) = delete;
	private:
		SubmitRecord* previous_;
	};

	struct WaitRecord
	{
		QueueGate* gate = nullptr;
		bool observed = false;
		uint64_t requested_timeout = 0;
		uint64_t witness_before = 0;
		uint64_t engine_before = 0;
		uint64_t engine_target = 0;
		VkResult initial = VK_NOT_READY;
		VkResult completion = VK_NOT_READY;
		int idle_calls = 0;
	};

	thread_local WaitRecord* active_record = nullptr;

	class ObserveWait
	{
	public:
		explicit ObserveWait(WaitRecord& record) : previous_(active_record) { active_record = &record; }
		~ObserveWait() { active_record = this->previous_; }
		ObserveWait(const ObserveWait&) = delete;
		ObserveWait& operator=(const ObserveWait&) = delete;
	private:
		WaitRecord* previous_;
	};

	struct Fixture
	{
		render_tests::TestWindow window{ initial_size, initial_size };
		RenderResources resources;
		std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();

		Fixture()
		{
			this->renderer->create_device(this->window.handle(), initial_size, initial_size, 1);
			this->renderer->set_resources(&this->resources);
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(*this->renderer, this->resources, "pending_submission", white);
			const TextureHandle texture = this->resources.resolve_texture("pending_submission");
			this->renderer->begin_frame();
			this->renderer->set_view_count(1);
			this->renderer->view(0).draw_sprite(texture, RectangleI(0, 0, 2, 2),
				RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::white, 0.0f,
				Vector2F::zero, SpriteFlip::none, 0.0f);
		}
	};

	void submit(Fixture& fixture, QueueGate& gate)
	{
		// Finish descriptors, buffer sizing and command recording before the
		// gate is queued; only execute's single actual submission is injected.
		fixture.renderer->submit();
		gate.block_queue();
		DeviceResources& device = fixture.renderer->impl()->device_resources;
		SubmitRecord record{ &gate, device.frame().commands };
		bool executed = false;
		{
			InjectSubmit injection(record);
			executed = device.execute();
		}
		REQUIRE(executed);
		CHECK(record.calls == 1);
		REQUIRE(record.injected == 1);
	}

	void check_observation(const WaitRecord& record, const QueueGate& gate)
	{
		REQUIRE(record.observed);
		CHECK(record.requested_timeout == UINT64_MAX);
		CHECK(record.witness_before == 0);
		CHECK(record.engine_before < record.engine_target);
		CHECK(record.initial == VK_TIMEOUT);
		CHECK(record.completion == VK_SUCCESS);
		CHECK(gate.witness() == 1);
		CHECK(gate.signal_calls() == 1);
		CHECK(gate.signal_result() == VK_SUCCESS);
	}
}

// A binary wait requires its signal and dependent signals to have already
// been submitted (VUID-vkQueueSubmit-pWaitSemaphores-03238). This test instead
// adds a timeline witness wait to the actual execute batch, allowing the
// future host gate signal. The command buffer and engine timeline signal are
// the production ones; submission instrumentation is part of this proof.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue,
	uint32_t count, const VkSubmitInfo* submissions, VkFence fence)
{
	const VulkanCalls& calls = real_calls();
	SubmitRecord* record = active_submit;
	if (record == nullptr)
	{
		return calls.submit(queue, count, submissions, fence);
	}
	QueueGate& gate = *record->gate;
	++record->calls;
	if (queue != gate.queue() || record->calls != 1 || count != 1 || submissions == nullptr ||
		submissions[0].sType != VK_STRUCTURE_TYPE_SUBMIT_INFO || submissions[0].pNext == nullptr ||
		submissions[0].waitSemaphoreCount != 0 || submissions[0].commandBufferCount != 1 ||
		submissions[0].pCommandBuffers == nullptr ||
		submissions[0].pCommandBuffers[0] != record->expected_commands ||
		submissions[0].signalSemaphoreCount != 1 || submissions[0].pSignalSemaphores == nullptr ||
		submissions[0].pSignalSemaphores[0] == VK_NULL_HANDLE)
	{
		gate.abort("unexpected execute submission shape", VK_ERROR_UNKNOWN);
	}
	// Inspect only the type header before reading the expected full structure.
	VkStructureType extension_type = VK_STRUCTURE_TYPE_MAX_ENUM;
	std::memcpy(&extension_type, submissions[0].pNext, sizeof(extension_type));
	if (extension_type != VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
	{
		gate.abort("unexpected execute extension chain", VK_ERROR_UNKNOWN);
	}
	const VkTimelineSemaphoreSubmitInfo* timeline =
		static_cast<const VkTimelineSemaphoreSubmitInfo*>(submissions[0].pNext);
	if (timeline->pNext != nullptr || timeline->waitSemaphoreValueCount != 0 ||
		timeline->signalSemaphoreValueCount != 1 ||
		timeline->pSignalSemaphoreValues == nullptr || timeline->pSignalSemaphoreValues[0] == 0)
	{
		gate.abort("unexpected execute timeline shape", VK_ERROR_UNKNOWN);
	}
	const VkSemaphore witness = gate.witness_semaphore();
	const uint64_t one = 1;
	const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkTimelineSemaphoreSubmitInfo injected_timeline = *timeline;
	injected_timeline.waitSemaphoreValueCount = 1;
	injected_timeline.pWaitSemaphoreValues = &one;
	VkSubmitInfo injected = submissions[0];
	injected.pNext = &injected_timeline;
	injected.waitSemaphoreCount = 1;
	injected.pWaitSemaphores = &witness;
	injected.pWaitDstStageMask = &stage;
	const VkResult result = calls.submit(queue, count, &injected, fence);
	if (result != VK_SUCCESS)
	{
		gate.abort("instrumented execute submission failed", result);
	}
	++record->injected;
	return result;
}

// Unarmed calls retain the actual arguments and timeout. Only these three
// scoped cases intercept waits; all other Vulkan pixel tests keep their waits.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphores(
	VkDevice device, const VkSemaphoreWaitInfo* wait, uint64_t timeout)
{
	const VulkanCalls& calls = real_calls();
	WaitRecord* record = active_record;
	if (record == nullptr)
	{
		return calls.wait(device, wait, timeout);
	}
	QueueGate& gate = *record->gate;
	if (device != gate.device())
	{
		gate.abort("unexpected device at semaphore wait", VK_ERROR_UNKNOWN);
	}
	const bool first = !record->observed;
	if (first)
	{
		if (wait == nullptr || wait->semaphoreCount != 1 || wait->pSemaphores == nullptr ||
			wait->pValues == nullptr || wait->flags != 0 || timeout != UINT64_MAX)
		{
			gate.abort("unexpected production semaphore wait shape", VK_ERROR_UNKNOWN);
		}
		record->requested_timeout = timeout;
		record->witness_before = gate.witness();
		record->engine_target = wait->pValues[0];
		VkResult result = calls.counter(device, wait->pSemaphores[0], &record->engine_before);
		if (result != VK_SUCCESS || record->witness_before != 0 ||
			record->engine_before >= record->engine_target)
		{
			gate.abort("engine wait was not pending", result, record->engine_before);
		}
		record->initial = calls.wait(device, wait, 0);
		if (record->initial != VK_TIMEOUT)
		{
			gate.abort("zero-time production wait was not pending", record->initial);
		}
		record->observed = true;
		gate.release_and_wait();
	}
	const VkResult result = calls.wait(device, wait, std::min(timeout, completion_timeout_ns));
	if (result != VK_SUCCESS)
	{
		gate.abort("bounded production semaphore wait failed", result);
	}
	if (first)
	{
		record->completion = result;
	}
	return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
{
	const VulkanCalls& calls = real_calls();
	WaitRecord* record = active_record;
	if (record == nullptr)
	{
		return calls.idle(device);
	}
	QueueGate& gate = *record->gate;
	if (device != gate.device())
	{
		gate.abort("unexpected device at idle wait", VK_ERROR_UNKNOWN);
	}
	const uint64_t witness = gate.witness();
	if (witness != (record->observed ? 1u : 0u))
	{
		gate.abort("unexpected witness at device idle entry", VK_ERROR_UNKNOWN, witness);
	}
	++record->idle_calls;
	gate.release_and_wait();
	// No API timeout exists here. The sole worker's absolute process deadline
	// remains armed through this call and final tail retirement.
	const VkResult result = calls.idle(device);
	if (result != VK_SUCCESS)
	{
		gate.abort("production device idle wait failed", result);
	}
	return result;
}

// These prove instrumented submission/wait ordering of real pending queue
// submissions. They do not prove unmodified submissions or infinite waits,
// two busy swapchain slots, physical GPU saturation or final device
// destruction while the shared owner is held.
TEST_CASE("Vulkan same-index begin_frame observes a pending submission wait")
{
	Fixture fixture;
	DeviceResources& device = fixture.renderer->impl()->device_resources;
	DeviceResources::Frame* frame = &device.frame();
	QueueGate gate(device);
	submit(fixture, gate);
	WaitRecord record{ &gate };
	{
		ObserveWait observation(record);
		REQUIRE(gate.witness() == 0);
		fixture.renderer->begin_frame();
	}
	gate.finish();
	check_observation(record, gate);
	CHECK(&device.frame() == frame);
}

TEST_CASE("Vulkan real hidden resize observes a pending submission wait")
{
	Fixture fixture;
	REQUIRE(fixture.window.resize(resized_size, resized_size));
	QueueGate gate(fixture.renderer->impl()->device_resources);
	submit(fixture, gate);
	WaitRecord record{ &gate };
	bool resized = false;
	{
		ObserveWait observation(record);
		REQUIRE(gate.witness() == 0);
		resized = fixture.renderer->window_size_changed(resized_size, resized_size);
	}
	gate.finish();
	check_observation(record, gate);
	CHECK(record.idle_calls > 0);
	CHECK(resized);
	CHECK(fixture.renderer->back_buffer_size() == Vector2F(
		static_cast<float>(resized_size), static_cast<float>(resized_size)));
}

TEST_CASE("Vulkan renderer teardown observes a pending submission wait")
{
	Fixture fixture;
	QueueGate gate(fixture.renderer->impl()->device_resources);
	submit(fixture, gate);
	WaitRecord record{ &gate };
	{
		ObserveWait observation(record);
		REQUIRE(gate.witness() == 0);
		fixture.renderer.reset();
	}
	gate.finish();
	check_observation(record, gate);
	CHECK(record.idle_calls > 0);
}

#include <doctest/doctest.h>
#include "tests/render/allocation_scope.h"
#include "tests/render/device_fixture.h"
#include "tests/render/vulkan/submit_loss.h"

#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/vulkan/backend.h"

#include <vulkan/vulkan.h>
#include <windows.h>

#include <bit>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace
{
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

	struct CleanupCalls
	{
		PFN_vkCreateImage create_image =
			loader_function<PFN_vkCreateImage>("vkCreateImage");
		PFN_vkDestroyImage destroy_image =
			loader_function<PFN_vkDestroyImage>("vkDestroyImage");
		PFN_vkBindImageMemory bind_image =
			loader_function<PFN_vkBindImageMemory>("vkBindImageMemory");
		PFN_vkCreateImageView create_view =
			loader_function<PFN_vkCreateImageView>("vkCreateImageView");
		PFN_vkDestroyImageView destroy_view =
			loader_function<PFN_vkDestroyImageView>("vkDestroyImageView");
		PFN_vkAllocateMemory allocate =
			loader_function<PFN_vkAllocateMemory>("vkAllocateMemory");
		PFN_vkFreeMemory free =
			loader_function<PFN_vkFreeMemory>("vkFreeMemory");
		PFN_vkCreateBuffer create_buffer =
			loader_function<PFN_vkCreateBuffer>("vkCreateBuffer");
		PFN_vkBindBufferMemory bind_buffer =
			loader_function<PFN_vkBindBufferMemory>("vkBindBufferMemory");
		PFN_vkDestroyBuffer destroy_buffer =
			loader_function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
		PFN_vkCreateSemaphore create_semaphore =
			loader_function<PFN_vkCreateSemaphore>("vkCreateSemaphore");
		PFN_vkDestroyDevice destroy_device =
			loader_function<PFN_vkDestroyDevice>("vkDestroyDevice");
	};

	const CleanupCalls& real_calls()
	{
		static const CleanupCalls calls;
		return calls;
	}

	// One captured handle: the device it was made on, and each destruction
	// of it. A driver may hand the same value to a later object once this one
	// is gone, so the value's liveness is followed rather than its count: a
	// destruction while the value is not live is a second release of an
	// object already gone, whether or not the value was reused in between.
	// destroys counts only the captured object's own life.
	template<typename Handle>
	struct Tracked
	{
		Handle handle = VK_NULL_HANDLE;
		VkDevice created_on = VK_NULL_HANDLE;
		int destroys = 0;
		VkDevice destroyed_on = VK_NULL_HANDLE;
		int destroyed_at = 0;
		bool live = false;
		bool reused = false;
		int released_while_dead = 0;

		void capture(Handle value, VkDevice device)
		{
			this->handle = value;
			this->created_on = device;
			this->live = true;
		}

		void observe_creation(Handle value)
		{
			if (value != VK_NULL_HANDLE && value == this->handle && this->destroys > 0)
			{
				this->reused = true;
				this->live = true;
			}
		}

		void observe_destruction(Handle value, VkDevice device, int& clock)
		{
			if (value == VK_NULL_HANDLE || value != this->handle)
			{
				return;
			}
			if (!this->live)
			{
				++this->released_while_dead;
				return;
			}
			this->live = false;
			if (!this->reused)
			{
				++this->destroys;
				this->destroyed_on = device;
				this->destroyed_at = ++clock;
			}
		}
	};

	struct CleanupProbe
	{
		// Capture the next texture image, the memory bound to it and its view.
		bool track_texture = false;
		Tracked<VkImage> image;
		Tracked<VkDeviceMemory> image_memory;
		Tracked<VkImageView> view;

		// Capture the next buffer and the memory bound to it.
		bool track_buffer = false;
		Tracked<VkBuffer> buffer;
		Tracked<VkDeviceMemory> buffer_memory;

		// When set, refuse the first semaphore created on any other device -
		// in recovery, the replacement device's timeline.
		VkDevice refuse_semaphore_off = VK_NULL_HANDLE;
		int semaphore_refusals = 0;
		VkDevice refused_on = VK_NULL_HANDLE;

		Tracked<VkDevice> device;
		int clock = 0;
	};

	thread_local CleanupProbe* active_probe = nullptr;

	class ProbeScope
	{
	public:
		explicit ProbeScope(CleanupProbe& probe)
		{
			std::ignore = real_calls();
			if (active_probe != nullptr)
			{
				throw std::logic_error("Vulkan cleanup probes cannot nest");
			}
			active_probe = &probe;
		}
		~ProbeScope() { active_probe = nullptr; }
		ProbeScope(const ProbeScope&) = delete;
		ProbeScope& operator=(const ProbeScope&) = delete;
	};

	// Arms the next submits to report device loss, and disarms on every way
	// out, so a failed case cannot hand an unconsumed loss to the next one.
	struct SubmitLossScope
	{
		explicit SubmitLossScope(int losses)
		{
			render_tests::reported_submit_losses = losses;
		}
		~SubmitLossScope() { render_tests::reported_submit_losses = 0; }
		SubmitLossScope(const SubmitLossScope&) = delete;
		SubmitLossScope& operator=(const SubmitLossScope&) = delete;
	};

	labrador::TextureData white_texture()
	{
		labrador::TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.levels.push_back(labrador::texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size, 255);
		return texture;
	}

	template<typename Handle>
	void check_destroyed_once(const Tracked<Handle>& tracked)
	{
		REQUIRE(tracked.handle != VK_NULL_HANDLE);
		CHECK(tracked.destroys == 1);
		CHECK(tracked.released_while_dead == 0);
		CHECK(tracked.destroyed_on == tracked.created_on);
	}
}

// These executable-local symbols forward to the DLL exports, not themselves.
// Each only observes unless its case has armed the thread's probe.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device,
	const VkImageCreateInfo* description, const VkAllocationCallbacks* allocator,
	VkImage* image)
{
	const VkResult result = real_calls().create_image(device, description, allocator, image);
	CleanupProbe* probe = active_probe;
	if (probe != nullptr && result == VK_SUCCESS)
	{
		probe->image.observe_creation(*image);
		if (probe->track_texture && probe->image.handle == VK_NULL_HANDLE)
		{
			probe->image.capture(*image, device);
		}
	}
	return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image,
	const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr)
	{
		active_probe->image.observe_destruction(image, device, active_probe->clock);
	}
	real_calls().destroy_image(device, image, allocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice device,
	VkImage image, VkDeviceMemory memory, VkDeviceSize offset)
{
	CleanupProbe* probe = active_probe;
	if (probe != nullptr && probe->track_texture && image == probe->image.handle &&
		probe->image_memory.handle == VK_NULL_HANDLE)
	{
		probe->image_memory.capture(memory, device);
	}
	return real_calls().bind_image(device, image, memory, offset);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice device,
	const VkImageViewCreateInfo* description, const VkAllocationCallbacks* allocator,
	VkImageView* view)
{
	const VkResult result = real_calls().create_view(device, description, allocator, view);
	CleanupProbe* probe = active_probe;
	if (probe != nullptr && result == VK_SUCCESS)
	{
		probe->view.observe_creation(*view);
		if (probe->track_texture && description->image == probe->image.handle &&
			probe->view.handle == VK_NULL_HANDLE)
		{
			probe->view.capture(*view, device);
		}
	}
	return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice device,
	VkImageView view, const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr)
	{
		active_probe->view.observe_destruction(view, device, active_probe->clock);
	}
	real_calls().destroy_view(device, view, allocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice device,
	const VkMemoryAllocateInfo* allocation, const VkAllocationCallbacks* allocator,
	VkDeviceMemory* memory)
{
	const VkResult result = real_calls().allocate(device, allocation, allocator, memory);
	if (active_probe != nullptr && result == VK_SUCCESS)
	{
		active_probe->image_memory.observe_creation(*memory);
		active_probe->buffer_memory.observe_creation(*memory);
	}
	return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice device,
	VkDeviceMemory memory, const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr)
	{
		active_probe->image_memory.observe_destruction(memory, device, active_probe->clock);
		active_probe->buffer_memory.observe_destruction(memory, device, active_probe->clock);
	}
	real_calls().free(device, memory, allocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
	const VkBufferCreateInfo* description, const VkAllocationCallbacks* allocator,
	VkBuffer* buffer)
{
	const VkResult result = real_calls().create_buffer(device, description, allocator, buffer);
	CleanupProbe* probe = active_probe;
	if (probe != nullptr && result == VK_SUCCESS)
	{
		probe->buffer.observe_creation(*buffer);
		if (probe->track_buffer && probe->buffer.handle == VK_NULL_HANDLE)
		{
			probe->buffer.capture(*buffer, device);
		}
	}
	return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device,
	VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset)
{
	CleanupProbe* probe = active_probe;
	if (probe != nullptr && probe->track_buffer && buffer == probe->buffer.handle &&
		probe->buffer_memory.handle == VK_NULL_HANDLE)
	{
		probe->buffer_memory.capture(memory, device);
	}
	return real_calls().bind_buffer(device, buffer, memory, offset);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer,
	const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr)
	{
		active_probe->buffer.observe_destruction(buffer, device, active_probe->clock);
	}
	real_calls().destroy_buffer(device, buffer, allocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice device,
	const VkSemaphoreCreateInfo* description, const VkAllocationCallbacks* allocator,
	VkSemaphore* semaphore)
{
	CleanupProbe* probe = active_probe;
	if (probe != nullptr && probe->refuse_semaphore_off != VK_NULL_HANDLE &&
		device != probe->refuse_semaphore_off && probe->semaphore_refusals == 0)
	{
		++probe->semaphore_refusals;
		probe->refused_on = device;
		*semaphore = VK_NULL_HANDLE;
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}
	return real_calls().create_semaphore(device, description, allocator, semaphore);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,
	const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr)
	{
		active_probe->device.observe_destruction(device, device, active_probe->clock);
	}
	real_calls().destroy_device(device, allocator);
}

// The refusal is a Debug CRT allocation hook, so this case exists only where
// allocation_scope.h does: a Debug build against the DLL CRT.
#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)
TEST_CASE("Vulkan texture wrapper allocation failure destroys its image, memory and view once")
{
	using namespace labrador;
	using render_tests::AllocationSample;
	using render_tests::AllocationScope;

	CleanupProbe probe;
	{
		// Observing through the fixture's teardown, so a second release by
		// any later owner would be counted.
		ProbeScope scope(probe);
		render_tests::DeviceFixture fixture;
		RenderResources::Impl& table = *fixture.resources.impl();
		add_texture_asset(fixture.renderer, fixture.resources, "vulkan_wrapper_a",
			white_texture());

		// Locate the wrapper in an identical successful load of a new name.
		AllocationSample located;
		located.watched_bytes = sizeof(VulkanTexture);
		{
			AllocationScope allocations(located);
			add_texture_asset(fixture.renderer, fixture.resources, "vulkan_wrapper_b",
				white_texture());
		}
		REQUIRE_FALSE(located.hook_conflict);
		REQUIRE(located.watched_attempts == 1);

		probe.track_texture = true;
		AllocationSample refused;
		refused.refused_attempt = located.watched_attempt;
		{
			AllocationScope allocations(refused);
			CHECK_THROWS_AS(add_texture_asset(fixture.renderer, fixture.resources,
				"vulkan_wrapper_c", white_texture()), std::bad_alloc);
		}
		probe.track_texture = false;
		REQUIRE_FALSE(refused.hook_conflict);
		CHECK(refused.refused == 1);
		CHECK(refused.refused_bytes == sizeof(VulkanTexture));

		// All three raw handles existed when the wrapper was refused, and the
		// loader's cleanup released each through its creating device before
		// the exception left it. Nothing was published.
		check_destroyed_once(probe.image);
		check_destroyed_once(probe.image_memory);
		check_destroyed_once(probe.view);
		CHECK(probe.view.destroyed_at < probe.image.destroyed_at);
		CHECK(probe.image_memory.destroyed_at < probe.image.destroyed_at);
		CHECK_FALSE(table.textures.contains("vulkan_wrapper_c"));
		CHECK_THROWS_AS(std::ignore = fixture.resources.resolve_texture("vulkan_wrapper_c"),
			std::out_of_range);

		// The loader still works for that name afterwards.
		add_texture_asset(fixture.renderer, fixture.resources, "vulkan_wrapper_c",
			white_texture());
		CHECK(table.textures.contains("vulkan_wrapper_c"));
	}

	// Teardown released every surviving texture without touching the refused
	// one's handles again.
	CHECK(probe.image.destroys == 1);
	CHECK(probe.image_memory.destroys == 1);
	CHECK(probe.view.destroys == 1);
	CHECK(probe.image.released_while_dead == 0);
	CHECK(probe.image_memory.released_while_dead == 0);
	CHECK(probe.view.released_while_dead == 0);
}
#endif

TEST_CASE("Vulkan read-back releases its buffer through the original device after reported loss")
{
	using namespace labrador;
	bool recovery_throws = false;
	SUBCASE("recovery succeeds") {}
	SUBCASE("recovery throws after a replacement device exists") { recovery_throws = true; }
	CAPTURE(recovery_throws);

	const std::vector<unsigned char> sentinel(64 * 64 * 4, 0xA5);
	CleanupProbe probe;
	{
		ProbeScope scope(probe);
		render_tests::DeviceFixture fixture;
		const VkDevice original =
			fixture.renderer.impl()->device_resources.shared_device()->device;
		REQUIRE(original != VK_NULL_HANDLE);
		probe.device.capture(original, original);

		fixture.renderer.begin_frame();
		fixture.renderer.submit();

		probe.track_buffer = true;
		if (recovery_throws)
		{
			probe.refuse_semaphore_off = original;
		}
		std::optional<SubmitLossScope> losses(std::in_place, 1);
		std::vector<unsigned char> pixels = sentinel;
		std::string message;
		try
		{
			fixture.renderer.read_back_buffer(pixels);
			FAIL("a reported loss during read-back did not throw");
		}
		catch (const std::runtime_error& error)
		{
			message = error.what();
		}
		const int unconsumed_losses = render_tests::reported_submit_losses;
		losses.reset();
		probe.track_buffer = false;
		probe.refuse_semaphore_off = VK_NULL_HANDLE;

		CHECK(unconsumed_losses == 0);
		if (recovery_throws)
		{
			// The recovery's own error reaches the caller, not the read-back's
			// lost-frame sentence.
			CHECK(message == "The frame timeline could not be created "
				"(VK_ERROR_OUT_OF_DEVICE_MEMORY).");
			CHECK(probe.semaphore_refusals == 1);
			CHECK(probe.refused_on != VK_NULL_HANDLE);
			CHECK(probe.refused_on != original);
		}
		else
		{
			CHECK(message.find("The device was lost while reading the back buffer") !=
				std::string::npos);
			CHECK(probe.semaphore_refusals == 0);
		}

		// No lost-frame pixel was read: resize kept the caller's bytes.
		CHECK(pixels == sentinel);

		// The read-back buffer and its memory were made on the original device
		// and released through it exactly once, before that device went.
		REQUIRE(probe.buffer.created_on == original);
		check_destroyed_once(probe.buffer);
		check_destroyed_once(probe.buffer_memory);
		CHECK(probe.buffer_memory.created_on == original);

		if (!recovery_throws)
		{
			// Recovery restored a working renderer.
			fixture.renderer.end_frame();
			fixture.renderer.begin_frame();
			fixture.renderer.submit();
			std::vector<unsigned char> recovered;
			CHECK_NOTHROW(fixture.renderer.read_back_buffer(recovered));
			CHECK(recovered.size() == sentinel.size());
			fixture.renderer.end_frame();
		}
	}

	CHECK(probe.buffer.destroys == 1);
	CHECK(probe.buffer_memory.destroys == 1);
	CHECK(probe.buffer.released_while_dead == 0);
	CHECK(probe.buffer_memory.released_while_dead == 0);
	CHECK(probe.device.destroys == 1);
	CHECK(probe.buffer.destroyed_at < probe.device.destroyed_at);
	CHECK(probe.buffer_memory.destroyed_at < probe.device.destroyed_at);
}

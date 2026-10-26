#include "tests/render/test_window.h"

#include "engine/render/renderer.h"

#include <doctest/doctest.h>
#include <vulkan/vulkan.h>
#include <windows.h>

#include <bit>
#include <stdexcept>
#include <string>

namespace
{
	PFN_vkCreateInstance real_create_instance()
	{
		static const PFN_vkCreateInstance create = []()
			{
				const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
				const FARPROC address = module == nullptr ? nullptr :
					GetProcAddress(module, "vkCreateInstance");
				if (address == nullptr)
				{
					throw std::runtime_error("Vulkan test export missing: vkCreateInstance");
				}
				static_assert(sizeof(PFN_vkCreateInstance) == sizeof(FARPROC));
				return std::bit_cast<PFN_vkCreateInstance>(address);
			}();
		return create;
	}

	struct InstanceProbe
	{
		VkResult failure = VK_SUCCESS;
		int calls = 0;
		int injected = 0;
		int forwarded = 0;
		bool output_started_null = false;
	};

	thread_local InstanceProbe* active_probe = nullptr;

	class ProbeScope
	{
	public:
		explicit ProbeScope(InstanceProbe& probe)
		{
			static_cast<void>(real_create_instance());
			if (active_probe != nullptr)
			{
				throw std::logic_error("Vulkan instance probes cannot nest");
			}
			active_probe = &probe;
		}
		~ProbeScope() { active_probe = nullptr; }
		ProbeScope(const ProbeScope&) = delete;
		ProbeScope& operator=(const ProbeScope&) = delete;
	};
}

// The executable owns this symbol; unarmed calls use the actual DLL export.
// Synthetic failures leave the output untouched and create no Vulkan object.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
	const VkInstanceCreateInfo* description, const VkAllocationCallbacks* allocator,
	VkInstance* instance)
{
	if (active_probe != nullptr)
	{
		++active_probe->calls;
		active_probe->output_started_null =
			instance != nullptr && *instance == VK_NULL_HANDLE;
		if (active_probe->calls == 1 && active_probe->failure != VK_SUCCESS)
		{
			++active_probe->injected;
			return active_probe->failure;
		}
		++active_probe->forwarded;
	}
	return real_create_instance()(description, allocator, instance);
}

TEST_CASE("Vulkan initialization forwards instance creation without an injected failure")
{
	render_tests::TestWindow window(64, 64);
	labrador::Renderer renderer;
	InstanceProbe probe;
	{
		ProbeScope scope(probe);
		renderer.create_device(window.handle(), 64, 64, 1);
	}
	CHECK(probe.calls == 1);
	CHECK(probe.injected == 0);
	CHECK(probe.forwarded == 1);
	CHECK(probe.output_started_null);
	CHECK_NOTHROW(renderer.device_info());
}

TEST_CASE("Vulkan initialization identifies instance creation and its failed result")
{
	VkResult failure = VK_ERROR_OUT_OF_HOST_MEMORY;
	const char* result_name = "VK_ERROR_OUT_OF_HOST_MEMORY";
	SUBCASE("host memory") {}
	SUBCASE("extension")
	{
		failure = VK_ERROR_EXTENSION_NOT_PRESENT;
		result_name = "VK_ERROR_EXTENSION_NOT_PRESENT";
	}
	SUBCASE("incompatible driver")
	{
		failure = VK_ERROR_INCOMPATIBLE_DRIVER;
		result_name = "VK_ERROR_INCOMPATIBLE_DRIVER";
	}
	CAPTURE(std::string(result_name));
	const std::string expected =
		std::string("The Vulkan instance could not be created (") + result_name + ").";
	render_tests::TestWindow window(64, 64);
	labrador::Renderer renderer;
	InstanceProbe probe;
	probe.failure = failure;
	bool runtime_error = false;
	std::string actual;
	{
		ProbeScope scope(probe);
		try
		{
			renderer.create_device(window.handle(), 64, 64, 1);
		}
		catch (const std::runtime_error& error)
		{
			runtime_error = true;
			actual = error.what();
		}
	}
	CHECK(runtime_error);
	CHECK(actual == expected);
	CHECK(probe.calls == 1);
	CHECK(probe.injected == 1);
	CHECK(probe.forwarded == 0);
	CHECK(probe.output_started_null);
}

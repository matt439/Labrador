#include <doctest/doctest.h>
#include "tests/render/test_window.h"

#include "engine/render/renderer.h"

#include <vulkan/vulkan.h>
#include <windows.h>

#include <bit>
#include <cstring>
#include <stdexcept>
#include <string>

namespace
{
	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr :
			GetProcAddress(module, name);
		if (address == nullptr)
		{
			throw std::runtime_error(std::string("Vulkan test export missing: ") + name);
		}
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}

	PFN_vkCreateInstance real_create_instance()
	{
		static const PFN_vkCreateInstance create =
			loader_function<PFN_vkCreateInstance>("vkCreateInstance");
		return create;
	}

	PFN_vkDestroyInstance real_destroy_instance()
	{
		static const PFN_vkDestroyInstance destroy =
			loader_function<PFN_vkDestroyInstance>("vkDestroyInstance");
		return destroy;
	}

	struct InstanceProbe
	{
		VkResult failure = VK_SUCCESS;
		int calls = 0;
		int injected = 0;
		int forwarded = 0;
		bool output_started_null = false;
		bool inspect_callbacks = false;
		bool validation_enabled = false;
		bool debug_utils_enabled = false;
		int lifecycle_descriptions = 0;
		VkDebugUtilsMessageSeverityFlagsEXT severities = 0;
		VkDebugUtilsMessageTypeFlagsEXT types = 0;
		PFN_vkDebugUtilsMessengerCallbackEXT callback = nullptr;
		void* user_data = nullptr;
		VkInstance instance = VK_NULL_HANDLE;
		int creation_callbacks = 0;
		int destruction_callbacks = 0;
		int destructions = 0;
		bool callbacks_continue = true;
	};

	thread_local InstanceProbe* active_probe = nullptr;

	void inspect_callbacks(InstanceProbe& probe, const VkInstanceCreateInfo& description)
	{
		for (uint32_t index = 0; index < description.enabledLayerCount; ++index)
		{
			if (std::strcmp(description.ppEnabledLayerNames[index],
				"VK_LAYER_KHRONOS_validation") == 0)
				probe.validation_enabled = true;
		}
		for (uint32_t index = 0; index < description.enabledExtensionCount; ++index)
		{
			if (std::strcmp(description.ppEnabledExtensionNames[index],
				VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
				probe.debug_utils_enabled = true;
		}
		const VkBaseInStructure* next =
			static_cast<const VkBaseInStructure*>(description.pNext);
		while (next != nullptr)
		{
			if (next->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT)
			{
				const VkDebugUtilsMessengerCreateInfoEXT& messenger =
					*reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(next);
				++probe.lifecycle_descriptions;
				probe.severities = messenger.messageSeverity;
				probe.types = messenger.messageType;
				probe.callback = messenger.pfnUserCallback;
				probe.user_data = messenger.pUserData;
			}
			next = next->pNext;
		}
	}

	void invoke_callback(InstanceProbe& probe, bool destroying)
	{
		if (probe.callback == nullptr)
			return;
		VkDebugUtilsMessengerCallbackDataEXT data = {};
		data.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
		data.pMessage = destroying ? "Synthetic instance destruction diagnostic" :
			"Synthetic instance creation diagnostic";
		const VkBool32 result = probe.callback(
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &data, probe.user_data);
		probe.callbacks_continue = probe.callbacks_continue && result == VK_FALSE;
		if (destroying)
			++probe.destruction_callbacks;
		else
			++probe.creation_callbacks;
	}

	void check_callback_contract(const InstanceProbe& probe)
	{
		CHECK(probe.debug_utils_enabled == probe.validation_enabled);
		CHECK(probe.lifecycle_descriptions == (probe.validation_enabled ? 1 : 0));
		if (probe.validation_enabled)
		{
			CHECK(probe.callback != nullptr);
			CHECK(probe.user_data == nullptr);
			CHECK(probe.severities == (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT));
			CHECK(probe.types == (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT));
		}
		else
		{
			CHECK(probe.callback == nullptr);
		}
		CHECK(probe.callbacks_continue);
	}

	class ProbeScope
	{
	public:
		explicit ProbeScope(InstanceProbe& probe)
		{
			static_cast<void>(real_create_instance());
			static_cast<void>(real_destroy_instance());
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
		if (active_probe->inspect_callbacks && description != nullptr)
		{
			inspect_callbacks(*active_probe, *description);
			invoke_callback(*active_probe, false);
		}
		if (active_probe->calls == 1 && active_probe->failure != VK_SUCCESS)
		{
			++active_probe->injected;
			return active_probe->failure;
		}
		++active_probe->forwarded;
	}
	const VkResult result = real_create_instance()(description, allocator, instance);
	if (active_probe != nullptr && active_probe->inspect_callbacks && result == VK_SUCCESS)
		active_probe->instance = *instance;
	return result;
}

// The probe invokes the copied callback at each API boundary. These markers
// test registration and callback lifetime, not actual layer-generated messages
// or the debugger's output. The real loader still creates/destroys the instance.
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
	VkInstance instance, const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr && active_probe->inspect_callbacks &&
		active_probe->instance == instance)
	{
		++active_probe->destructions;
		invoke_callback(*active_probe, true);
	}
	real_destroy_instance()(instance, allocator);
}

TEST_CASE("Vulkan instance diagnostics cover creation and destruction without owner data")
{
	render_tests::TestWindow window(64, 64);
	InstanceProbe probe;
	probe.inspect_callbacks = true;
	{
		ProbeScope scope(probe);
		labrador::Renderer renderer;
		renderer.create_device(window.handle(), 64, 64, 1);
	}
	check_callback_contract(probe);
	CHECK(probe.creation_callbacks == (probe.validation_enabled ? 1 : 0));
	CHECK(probe.destruction_callbacks == (probe.validation_enabled ? 1 : 0));
	CHECK(probe.destructions == 1);
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
	probe.inspect_callbacks = true;
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
	check_callback_contract(probe);
	CHECK(probe.creation_callbacks == (probe.validation_enabled ? 1 : 0));
	CHECK(probe.destruction_callbacks == 0);
	CHECK(probe.destructions == 0);
}

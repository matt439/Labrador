#include <doctest/doctest.h>
#include "tests/render/test_window.h"

#include "engine/render/renderer.h"

#include <vulkan/vulkan.h>
#include <windows.h>

#include <bit>
#include <stdexcept>
#include <string>

namespace
{
	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
			throw std::runtime_error(std::string("Vulkan test export missing: ") + name);
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}

	struct ShaderCalls
	{
		PFN_vkCreateShaderModule create =
			loader_function<PFN_vkCreateShaderModule>("vkCreateShaderModule");
		PFN_vkDestroyShaderModule destroy =
			loader_function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
	};

	const ShaderCalls& real_calls()
	{
		static const ShaderCalls calls;
		return calls;
	}

	struct ShaderProbe
	{
		int creates = 0;
		int injected = 0;
		int destroys = 0;
		VkDevice creating_device = VK_NULL_HANDLE;
		VkDevice destroying_device = VK_NULL_HANDLE;
		VkShaderModule vertex = VK_NULL_HANDLE;
	};

	thread_local ShaderProbe* active_probe = nullptr;

	class ProbeScope
	{
	public:
		explicit ProbeScope(ShaderProbe& probe) : calls_(real_calls()), probe_(probe)
		{
			if (active_probe != nullptr)
				throw std::logic_error("Vulkan shader probes cannot nest");
			active_probe = &probe;
		}
		~ProbeScope()
		{
			active_probe = nullptr;
			// A failing baseline is measured first, then releases its leaked
			// local while the renderer still owns the creating device.
			if (this->probe_.vertex != VK_NULL_HANDLE && this->probe_.destroys == 0)
				this->calls_.destroy(this->probe_.creating_device, this->probe_.vertex, nullptr);
		}
		ProbeScope(const ProbeScope&) = delete;
		ProbeScope& operator=(const ProbeScope&) = delete;

	private:
		const ShaderCalls& calls_;
		ShaderProbe& probe_;
	};
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
	const VkShaderModuleCreateInfo* description, const VkAllocationCallbacks* allocator,
	VkShaderModule* shader)
{
	ShaderProbe* probe = active_probe;
	if (probe != nullptr && ++probe->creates == 2)
	{
		++probe->injected;
		*shader = VK_NULL_HANDLE;
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}
	const VkResult result = real_calls().create(device, description, allocator, shader);
	if (probe != nullptr && result == VK_SUCCESS && probe->creates == 1)
	{
		probe->vertex = *shader;
		probe->creating_device = device;
	}
	return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device,
	VkShaderModule shader, const VkAllocationCallbacks* allocator)
{
	if (active_probe != nullptr && shader == active_probe->vertex)
	{
		++active_probe->destroys;
		active_probe->destroying_device = device;
	}
	real_calls().destroy(device, shader, allocator);
}

TEST_CASE("Vulkan pixel shader creation failure releases the successful vertex module")
{
	render_tests::TestWindow window{ 64, 64 };
	labrador::Renderer renderer;
	ShaderProbe probe;
	ProbeScope scope(probe);
	try
	{
		renderer.create_device(window.handle(), 64, 64, 2);
		FAIL("pixel shader failure did not escape device construction");
	}
	catch (const std::runtime_error& error)
	{
		const std::string message(error.what());
		CHECK(message.find("The sprite pixel shader could not be created") != std::string::npos);
		CHECK(message.find("VK_ERROR_OUT_OF_HOST_MEMORY") != std::string::npos);
	}
	CHECK(probe.creates == 2);
	CHECK(probe.injected == 1);
	REQUIRE(probe.vertex != VK_NULL_HANDLE);
	CHECK(probe.destroys == 1);
	CHECK(probe.destroying_device == probe.creating_device);
}

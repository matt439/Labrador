#include "tests/render/device_fixture.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#include <doctest/doctest.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace
{
	class QueryInstance
	{
	public:
		QueryInstance()
		{
			VkApplicationInfo application = {};
			application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			application.apiVersion = VK_API_VERSION_1_2;
			VkInstanceCreateInfo description = {};
			description.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			description.pApplicationInfo = &application;
			if (vkCreateInstance(&description, nullptr, &handle) != VK_SUCCESS)
				throw std::runtime_error("Vulkan limit query instance failed");
		}
		~QueryInstance() { vkDestroyInstance(handle, nullptr); }
		QueryInstance(const QueryInstance&) = delete;
		QueryInstance& operator=(const QueryInstance&) = delete;
		VkInstance handle = VK_NULL_HANDLE;
	};

	VkImageFormatProperties selected_limits(const labrador::RenderDeviceInfo& selected)
	{
		QueryInstance query;
		uint32_t count = 0;
		REQUIRE(vkEnumeratePhysicalDevices(query.handle, &count, nullptr) == VK_SUCCESS);
		std::vector<VkPhysicalDevice> devices(count);
		REQUIRE(vkEnumeratePhysicalDevices(query.handle, &count, devices.data()) == VK_SUCCESS);
		VkImageFormatProperties limits = {};
		bool matched = false;
		for (VkPhysicalDevice device : devices)
		{
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(device, &properties);
			if (properties.vendorID != selected.vendor_id ||
				properties.deviceID != selected.device_id ||
				selected.device_name != properties.deviceName) continue;
			VkImageFormatProperties current = {};
			REQUIRE(vkGetPhysicalDeviceImageFormatProperties(device,
				VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0,
				&current) == VK_SUCCESS);
			if (matched)
			{
				REQUIRE(current.maxExtent.width == limits.maxExtent.width);
				REQUIRE(current.maxExtent.height == limits.maxExtent.height);
			}
			limits = current;
			matched = true;
		}
		REQUIRE(matched);
		return limits;
	}
}

TEST_CASE("Vulkan refuses queried image extent overflow before creating an image")
{
	using namespace labrador;
	render_tests::DeviceFixture fixture;
	const VkImageFormatProperties limits = selected_limits(fixture.renderer.device_info());
	for (bool wide : { true, false })
	{
		TextureData texture;
		texture.width = wide ? static_cast<int>(limits.maxExtent.width + 1) : 1;
		texture.height = wide ? 1 : static_cast<int>(limits.maxExtent.height + 1);
		texture.levels.push_back(texture_level(texture.format, texture.width, texture.height, 0));
		texture.pixels.assign(texture.levels[0].size, 255);
		try
		{
			add_texture_asset(fixture.renderer, fixture.resources, "oversized-atlas", texture);
			FAIL("oversized image reached the driver");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message = error.what();
			CHECK(message.find("oversized-atlas") != std::string::npos);
			CHECK(message.find("image extent limit") != std::string::npos);
		}
	}
}

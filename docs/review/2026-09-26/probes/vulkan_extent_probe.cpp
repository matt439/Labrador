#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require_vk(VkResult result, const char* operation)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string(operation) + " failed with " +
                std::to_string(static_cast<int>(result)));
    }

    struct HiddenWindow
    {
        HWND handle = nullptr;
        HiddenWindow()
        {
            // No WS_VISIBLE and no ShowWindow: no focus or visible-window changes.
            this->handle = CreateWindowExW(0, L"STATIC", L"Labrador extent probe",
                WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr,
                GetModuleHandleW(nullptr), nullptr);
            if (this->handle == nullptr)
                throw std::runtime_error("Could not create the hidden probe window.");
        }
        ~HiddenWindow() { if (this->handle != nullptr) DestroyWindow(this->handle); }
        HiddenWindow(const HiddenWindow&) = delete;
        HiddenWindow& operator=(const HiddenWindow&) = delete;
    };

    struct QueryInstance
    {
        VkInstance handle = VK_NULL_HANDLE;
        QueryInstance()
        {
            VkApplicationInfo application = {};
            application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application.pApplicationName = "Labrador extent limits query";
            application.apiVersion = VK_API_VERSION_1_2;
            VkInstanceCreateInfo description = {};
            description.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            description.pApplicationInfo = &application;
            require_vk(vkCreateInstance(&description, nullptr, &this->handle),
                "query vkCreateInstance");
        }
        ~QueryInstance()
        {
            if (this->handle != VK_NULL_HANDLE) vkDestroyInstance(this->handle, nullptr);
        }
        QueryInstance(const QueryInstance&) = delete;
        QueryInstance& operator=(const QueryInstance&) = delete;
    };

    uint32_t selected_max_width(const labrador::RenderDeviceInfo& selected)
    {
        // Public device_info binds this independent read-only query to the device
        // Labrador actually selected. No renderer backend header is included.
        QueryInstance query;
        uint32_t count = 0;
        require_vk(vkEnumeratePhysicalDevices(query.handle, &count, nullptr),
            "query device count");
        std::vector<VkPhysicalDevice> devices(count);
        require_vk(vkEnumeratePhysicalDevices(query.handle, &count, devices.data()),
            "query devices");
        uint32_t width = 0;
        unsigned int matches = 0;
        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(device, &properties);
            if (properties.vendorID != selected.vendor_id ||
                properties.deviceID != selected.device_id ||
                selected.device_name != properties.deviceName)
                continue;

            VkImageFormatProperties limits = {};
            require_vk(vkGetPhysicalDeviceImageFormatProperties(device,
                VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                0, &limits), "query RGBA texture limits");
            if (matches != 0 && width != limits.maxExtent.width)
                throw std::runtime_error("Device identity is ambiguous across different limits.");
            width = limits.maxExtent.width;
            ++matches;
        }
        if (matches == 0)
            throw std::runtime_error("Public renderer device_info did not match a queried device.");
        std::printf("matched_devices=%u max_extent_width=%u\n", matches, width);
        return width;
    }
}

int main()
{
    try
    {
        // Layer settings must request LOG_MSG and FAIL. FAIL makes the validation
        // layer reject invalid API calls before they reach the driver.
        uint32_t layer_count = 0;
        require_vk(vkEnumerateInstanceLayerProperties(&layer_count, nullptr),
            "validation layer count");
        std::vector<VkLayerProperties> layers(layer_count);
        require_vk(vkEnumerateInstanceLayerProperties(&layer_count, layers.data()),
            "validation layer list");
        bool validation_available = false;
        for (const VkLayerProperties& layer : layers)
            if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
                validation_available = true;
        if (!validation_available)
            throw std::runtime_error("Refusing extent probe without Khronos validation installed.");

        HiddenWindow window;
        labrador::RenderResources resources;
        labrador::Renderer renderer;
        renderer.create_device(window.handle, 64, 64, 1);
        renderer.set_resources(&resources);
        const labrador::RenderDeviceInfo& selected = renderer.device_info();
        std::printf("backend=%s device=%s vendor=%u device_id=%u\n",
            selected.backend.c_str(), selected.device_name.c_str(),
            selected.vendor_id, selected.device_id);
        if (selected.backend != "vulkan")
            throw std::runtime_error("This probe must link the Vulkan engine configuration.");

        const uint32_t maximum_width = selected_max_width(selected);
        if (maximum_width == 0 || maximum_width >= 1048576u)
            throw std::runtime_error("Refusing a probe outside the four-megabyte payload bound.");

        labrador::TextureData texture;
        texture.width = static_cast<int>(maximum_width + 1u);
        texture.height = 1;
        texture.levels.push_back(labrador::texture_level(texture.format,
            texture.width, texture.height, 0));
        texture.pixels.assign(texture.levels[0].size, 255);
        std::printf("FACTORY_BEGIN width=%d height=%d bytes=%zu\n",
            texture.width, texture.height, texture.pixels.size());
        std::fflush(stdout);
        try
        {
            labrador::add_texture_asset(renderer, resources,
                "review_extent_one_beyond_limit", texture);
            std::printf("FACTORY_ACCEPTED\n");
        }
        catch (const std::exception& error)
        {
            std::printf("FACTORY_THROW=%s\n", error.what());
        }
        std::printf("FACTORY_END\n");
        std::fflush(stdout);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "PROBE_SETUP_ERROR=%s\n", error.what());
        return 2;
    }
    return 0;
}

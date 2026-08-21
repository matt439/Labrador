#include "engine/render/vulkan/device_resources.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace labrador
{
	namespace
	{
		// THE FLOOR, AND IT IS THE HIGHEST OF THE FIVE.
		//
		// engine/render/d3d11/device_resources.h puts this engine's floor at
		// feature level 10.0, the D3D12 one at 11_0 and Windows 10, and the GL
		// one at 3.3 core - 2010 hardware. This is Vulkan 1.2, which is 2020,
		// and the reason is one feature rather than a wish: a timeline
		// semaphore. That is what makes this backend's synchronisation the same
		// three rules D3D12's is, in the same shapes, instead of a lattice of
		// binary semaphores and fences that says the same thing in more places
		// (device_resources.h says so at length).
		//
		// IT IS NOT WHERE THE PLATFORM ARGUMENT LIVES. docs/port/android.md
		// wants this backend for Android, Linux and - through MoltenVK - the
		// Apple platforms; Android has shipped 1.1 since Android 10 and 1.3 on
		// current devices, and MoltenVK reports 1.2. So the floor is a desktop
		// driver from 2020 and a phone from 2019, which is comfortably below
		// the tier a port would target and is worth stating rather than
		// discovering.
		const uint32_t MIN_API_VERSION = VK_API_VERSION_1_2;

		// How many descriptor sets one pool holds. A set is one run - one
		// texture, one filter, one viewport - and a frame that wants more takes
		// another pool and keeps it. Sixty-four is a guess at the common case
		// and is not load-bearing in either direction: too small costs one
		// allocation on the first frame that needs it, too large costs a few
		// kilobytes that are never touched.
		const uint32_t SETS_PER_POOL = 64;

		// What the swapchain must be able to be the destination of, because the
		// frame is blitted into it rather than drawn into it.
		const VkImageUsageFlags SWAPCHAIN_USAGE =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		// The stage and the access a barrier out of this layout has to name.
		// Kept as one pair of functions rather than written at each barrier,
		// because a barrier whose source mask does not match the layout it is
		// leaving is the class of mistake that produces a frame that is right
		// on one driver and torn on another.
		VkPipelineStageFlags stage_of(VkImageLayout layout)
		{
			switch (layout)
			{
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			case VK_IMAGE_LAYOUT_UNDEFINED:
			default:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			}
		}

		VkAccessFlags access_of(VkImageLayout layout)
		{
			switch (layout)
			{
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// READ AS WELL AS WRITE, because the blend equation reads what
				// is already there - the destination factor is
				// ONE_MINUS_SRC_ALPHA and not ZERO, which is a term
				// RenderPixelTests pins by name.
				return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_ACCESS_TRANSFER_READ_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_ACCESS_TRANSFER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_ACCESS_SHADER_READ_BIT;
			case VK_IMAGE_LAYOUT_UNDEFINED:
			default:
				return 0;
			}
		}

		VkImageMemoryBarrier image_barrier(VkImage image, VkImageLayout from,
			VkImageLayout to)
		{
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = access_of(from);
			barrier.dstAccessMask = access_of(to);
			barrier.oldLayout = from;
			barrier.newLayout = to;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			return barrier;
		}

#if defined(_DEBUG)
		// The validation layers' own words, forwarded to the debugger.
		//
		// THIS IS WHY THE FIFTH BACKEND IS CHEAPER TO GET RIGHT THAN THE
		// FOURTH WAS. Direct3D's debug layer says what is wrong through a
		// message queue the D3D12 file asks to break on; Vulkan's says it
		// through a callback, and it checks strictly more - every barrier,
		// every layout, every descriptor a draw names - because the API has no
		// runtime to check them for you. A backend written against a silent
		// validation layer is a backend nobody has checked.
		VKAPI_ATTR VkBool32 VKAPI_CALL report(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* data,
			void* user)
		{
			std::ignore = severity;
			std::ignore = type;
			std::ignore = user;

			if (data != nullptr && data->pMessage != nullptr)
			{
				OutputDebugStringA("Vulkan: ");
				OutputDebugStringA(data->pMessage);
				OutputDebugStringA("\n");
			}

			// FALSE, ALWAYS. Returning true aborts the call that raised the
			// message, which is documented as being for layer development
			// rather than for applications.
			return VK_FALSE;
		}

		bool layer_available(const char* name)
		{
			uint32_t count = 0;
			if (vkEnumerateInstanceLayerProperties(&count, nullptr) !=
				VK_SUCCESS)
			{
				return false;
			}

			std::vector<VkLayerProperties> layers(count);
			if (vkEnumerateInstanceLayerProperties(&count, layers.data()) !=
				VK_SUCCESS)
			{
				return false;
			}

			for (const VkLayerProperties& layer : layers)
			{
				if (std::string(layer.layerName) == name)
				{
					return true;
				}
			}
			return false;
		}
#endif
	}

	std::string vk_result_name(VkResult result)
	{
		switch (result)
		{
		case VK_SUCCESS:                        return "VK_SUCCESS";
		case VK_NOT_READY:                      return "VK_NOT_READY";
		case VK_TIMEOUT:                        return "VK_TIMEOUT";
		case VK_INCOMPLETE:                     return "VK_INCOMPLETE";
		case VK_SUBOPTIMAL_KHR:                 return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST:              return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR:          return "VK_ERROR_OUT_OF_DATE_KHR";
		default:
			return "VkResult " + std::to_string(static_cast<int>(result));
		}
	}

	void check_vk(VkResult result, const char* what)
	{
		if (result == VK_SUCCESS)
		{
			return;
		}

		throw std::runtime_error(
			std::string(what) + " (" + vk_result_name(result) + ").");
	}

	// --- VulkanDevice --------------------------------------------------------

	VulkanDevice::~VulkanDevice()
	{
		// IN THIS ORDER AND NOT THE DECLARATION ORDER, which is why this is
		// written out rather than left to the compiler: a VkDevice is destroyed
		// with the instance that made it still alive, and a debug messenger is
		// an instance object that has to go before the instance it reports for.
		//
		// NOTHING WAITS HERE, because whoever gets to this point has already
		// waited. This object outlives DeviceResources by exactly as long as
		// some texture still names it, and DeviceResources' own destructor is
		// what waits for the GPU - so by the time the last shared_ptr goes, the
		// only thing left to do is free handles.
		if (this->device != VK_NULL_HANDLE)
		{
			vkDestroyDevice(this->device, nullptr);
			this->device = VK_NULL_HANDLE;
		}

		if (this->messenger != VK_NULL_HANDLE)
		{
			PFN_vkDestroyDebugUtilsMessengerEXT destroy =
				reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
					vkGetInstanceProcAddr(this->instance,
						"vkDestroyDebugUtilsMessengerEXT"));
			if (destroy != nullptr)
			{
				destroy(this->instance, this->messenger, nullptr);
			}
			this->messenger = VK_NULL_HANDLE;
		}

		if (this->instance != VK_NULL_HANDLE)
		{
			vkDestroyInstance(this->instance, nullptr);
			this->instance = VK_NULL_HANDLE;
		}
	}

	uint32_t VulkanDevice::memory_type(uint32_t allowed,
		VkMemoryPropertyFlags wanted, const char* what) const
	{
		for (uint32_t i = 0; i < this->memory_properties.memoryTypeCount; i++)
		{
			const bool allowed_here = (allowed & (1u << i)) != 0u;
			const VkMemoryPropertyFlags flags =
				this->memory_properties.memoryTypes[i].propertyFlags;

			if (allowed_here && (flags & wanted) == wanted)
			{
				return i;
			}
		}

		throw std::runtime_error(std::string(what) + " needs memory this "
			"device does not have a type for. Vulkan makes the heaps a caller's "
			"business, so this is where 'the device will not take it' is said "
			"rather than an allocation failing with a number.");
	}

	// --- VulkanBuffer --------------------------------------------------------

	VulkanBuffer create_vulkan_buffer(const VulkanDevice& owner,
		VkDeviceSize bytes, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memory, const char* what)
	{
		VulkanBuffer result;
		result.bytes = bytes;

		VkBufferCreateInfo description = {};
		description.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		description.size = bytes;
		description.usage = usage;
		description.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		check_vk(vkCreateBuffer(owner.device, &description, nullptr,
			&result.buffer), what);

		VkMemoryRequirements requirements = {};
		vkGetBufferMemoryRequirements(owner.device, result.buffer,
			&requirements);

		VkMemoryAllocateInfo allocation = {};
		allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocation.allocationSize = requirements.size;
		allocation.memoryTypeIndex = owner.memory_type(
			requirements.memoryTypeBits, memory, what);

		const VkResult allocated = vkAllocateMemory(owner.device, &allocation,
			nullptr, &result.memory);
		if (allocated != VK_SUCCESS)
		{
			vkDestroyBuffer(owner.device, result.buffer, nullptr);
			result.buffer = VK_NULL_HANDLE;
			check_vk(allocated, what);
		}

		check_vk(vkBindBufferMemory(owner.device, result.buffer, result.memory,
			0), what);

		// MAPPED ONCE AND NEVER UNMAPPED, for the host-visible ones. That is
		// the documented way to write from the CPU in this API and it is what
		// the D3D12 backend's upload-heap pages do; a map per frame would be a
		// call per frame to say the same thing.
		if ((memory & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u)
		{
			check_vk(vkMapMemory(owner.device, result.memory, 0, VK_WHOLE_SIZE,
				0, &result.mapped), what);
		}

		return result;
	}

	void destroy_vulkan_buffer(VkDevice device, VulkanBuffer& buffer)
	{
		if (device == VK_NULL_HANDLE)
		{
			buffer = VulkanBuffer{};
			return;
		}

		if (buffer.mapped != nullptr)
		{
			vkUnmapMemory(device, buffer.memory);
			buffer.mapped = nullptr;
		}
		if (buffer.buffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(device, buffer.buffer, nullptr);
			buffer.buffer = VK_NULL_HANDLE;
		}
		if (buffer.memory != VK_NULL_HANDLE)
		{
			vkFreeMemory(device, buffer.memory, nullptr);
			buffer.memory = VK_NULL_HANDLE;
		}
		buffer.bytes = 0;
	}

	// --- DeviceResources -----------------------------------------------------

	DeviceResources::DeviceResources()
	{

	}

	DeviceResources::~DeviceResources()
	{
		// ONE OF THE DESTRUCTORS IN engine/render/ THAT HAS TO SYNCHRONISE WITH
		// A GPU, and the reason is the one this whole file exists for. Nothing
		// here may be freed while the GPU is still reading it, and this API
		// tracks none of that for you.
		//
		// AND IT ANSWERS RATHER THAN THROWS. This destructor is implicitly
		// noexcept, so a throw out of the wait would be std::terminate on an
		// ordinary exit and the wait it was raised from would not have happened
		// either way. T6: teardown stays silent.
		std::ignore = this->try_wait_for_gpu();

		if (this->owner_ && this->owner_->device != VK_NULL_HANDLE)
		{
			std::ignore = vkDeviceWaitIdle(this->owner_->device);
		}

		this->destroy_swapchain();
		this->destroy_colour_target();
		this->destroy_frames();

		if (this->owner_)
		{
			VkDevice device = this->owner_->device;

			if (this->render_pass_ != VK_NULL_HANDLE)
			{
				vkDestroyRenderPass(device, this->render_pass_, nullptr);
				this->render_pass_ = VK_NULL_HANDLE;
			}
			if (this->upload_pool_ != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(device, this->upload_pool_, nullptr);
				this->upload_pool_ = VK_NULL_HANDLE;
			}
			if (this->timeline_ != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, this->timeline_, nullptr);
				this->timeline_ = VK_NULL_HANDLE;
			}
			if (this->surface_ != VK_NULL_HANDLE)
			{
				// Before the instance, which the shared owner destroys - and
				// this object is still holding a reference to it, which is what
				// makes that ordering true rather than lucky.
				vkDestroySurfaceKHR(this->owner_->instance, this->surface_,
					nullptr);
				this->surface_ = VK_NULL_HANDLE;
			}
		}

		this->owner_.reset();
	}

	void DeviceResources::set_window(HWND window, int width, int height)
	{
		this->window_ = window;

		this->output_size_.left = 0;
		this->output_size_.top = 0;
		this->output_size_.right = static_cast<long>(width);
		this->output_size_.bottom = static_cast<long>(height);
	}

	void DeviceResources::create_instance()
	{
		VkApplicationInfo application = {};
		application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		application.pApplicationName = "Labrador";
		application.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		application.pEngineName = "Labrador";
		application.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		application.apiVersion = MIN_API_VERSION;

		std::vector<const char*> extensions;
		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

		std::vector<const char*> layers;

#if defined(_DEBUG)
		// The validation layers are an optional install (they ship with the
		// Vulkan SDK), so this asks and carries on - the same shape the D3D12
		// file's debug-layer probe has, and for the same reason: a machine
		// without them is the normal case for a shipped build and is not a
		// reason to refuse to run.
		const bool validation =
			layer_available("VK_LAYER_KHRONOS_validation");
		if (validation)
		{
			layers.push_back("VK_LAYER_KHRONOS_validation");
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		else
		{
			OutputDebugStringA(
				"WARNING: Vulkan validation layers are not available\n");
		}
#endif

		VkInstanceCreateInfo description = {};
		description.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		description.pApplicationInfo = &application;
		description.enabledExtensionCount =
			static_cast<uint32_t>(extensions.size());
		description.ppEnabledExtensionNames = extensions.data();
		description.enabledLayerCount = static_cast<uint32_t>(layers.size());
		description.ppEnabledLayerNames = layers.data();

		const VkResult created = vkCreateInstance(&description, nullptr,
			&this->owner_->instance);
		if (created != VK_SUCCESS)
		{
			throw std::runtime_error("There is no usable Vulkan loader on this "
				"machine (" + vk_result_name(created) + "). vulkan-1.dll ships "
				"with a graphics driver and with the Vulkan SDK's runtime; a "
				"machine with neither has no Vulkan at all.");
		}

#if defined(_DEBUG)
		if (validation)
		{
			PFN_vkCreateDebugUtilsMessengerEXT create =
				reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
					vkGetInstanceProcAddr(this->owner_->instance,
						"vkCreateDebugUtilsMessengerEXT"));

			if (create != nullptr)
			{
				VkDebugUtilsMessengerCreateInfoEXT messenger = {};
				messenger.sType =
					VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
				messenger.messageSeverity =
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				messenger.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
				messenger.pfnUserCallback = report;

				std::ignore = create(this->owner_->instance, &messenger,
					nullptr, &this->owner_->messenger);
			}
		}
#endif
	}

	void DeviceResources::select_physical_device()
	{
		uint32_t count = 0;
		check_vk(vkEnumeratePhysicalDevices(this->owner_->instance, &count,
			nullptr), "The Vulkan devices on this machine could not be listed");

		if (count == 0)
		{
			// THE FAILURE A GPU-LESS BUILD MACHINE SEES, NAMED (T6). Direct3D
			// has WARP in the box and falls back to it; Vulkan's software
			// implementations - lavapipe, SwiftShader - are installed rather
			// than shipped, which is why .github/workflows/ci.yml skips
			// RenderPixelTests on this preset exactly as it does on the GL one.
			throw std::runtime_error("This machine has a Vulkan loader but no "
				"Vulkan device. There is no in-box software implementation the "
				"way Direct3D has WARP; a GPU driver or an installed software "
				"ICD (lavapipe, SwiftShader) is what provides one.");
		}

		std::vector<VkPhysicalDevice> devices(count);
		check_vk(vkEnumeratePhysicalDevices(this->owner_->instance, &count,
			devices.data()),
			"The Vulkan devices on this machine could not be listed");

		// The best device that can do everything this backend needs, where
		// "best" is discrete over anything else - the same preference the D3D12
		// backend's adapter walk has, expressed in this API's vocabulary.
		int best_score = -1;
		std::string refusals;

		for (VkPhysicalDevice candidate : devices)
		{
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(candidate, &properties);

			if (properties.apiVersion < MIN_API_VERSION)
			{
				refusals += std::string("\n  ") + properties.deviceName +
					": Vulkan " +
					std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) +
					"." +
					std::to_string(VK_VERSION_MINOR(properties.apiVersion)) +
					", and this backend needs 1.2 for timeline semaphores.";
				continue;
			}

			// A queue family that can draw and can present to this surface.
			// They are the same family on every desktop driver and are not
			// required to be, so this asks rather than assumes.
			uint32_t families = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families,
				nullptr);
			std::vector<VkQueueFamilyProperties> family_properties(families);
			vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families,
				family_properties.data());

			uint32_t chosen_family = families;
			for (uint32_t i = 0; i < families; i++)
			{
				if ((family_properties[i].queueFlags &
					VK_QUEUE_GRAPHICS_BIT) == 0u)
				{
					continue;
				}

				VkBool32 presents = VK_FALSE;
				if (vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i,
					this->surface_, &presents) != VK_SUCCESS ||
					presents != VK_TRUE)
				{
					continue;
				}

				chosen_family = i;
				break;
			}

			if (chosen_family == families)
			{
				refusals += std::string("\n  ") + properties.deviceName +
					": no queue family both draws and presents to this window.";
				continue;
			}

			VkPhysicalDeviceVulkan12Features twelve = {};
			twelve.sType =
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

			VkPhysicalDeviceFeatures2 features = {};
			features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			features.pNext = &twelve;
			vkGetPhysicalDeviceFeatures2(candidate, &features);

			if (twelve.timelineSemaphore != VK_TRUE)
			{
				refusals += std::string("\n  ") + properties.deviceName +
					": no timelineSemaphore, which is core in 1.2 and is what "
					"this backend's frame pacing is written against.";
				continue;
			}

			const int score = properties.deviceType ==
				VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
			if (score <= best_score)
			{
				continue;
			}

			best_score = score;
			this->owner_->physical_device = candidate;
			this->owner_->queue_family = chosen_family;
			this->owner_->block_compression =
				features.features.textureCompressionBC == VK_TRUE;
			this->owner_->uniform_alignment = std::max<VkDeviceSize>(
				properties.limits.minUniformBufferOffsetAlignment,
				sizeof(float) * 4);
		}

		if (this->owner_->physical_device == VK_NULL_HANDLE)
		{
			throw std::runtime_error("No Vulkan device on this machine can run "
				"this backend:" + refusals);
		}

		vkGetPhysicalDeviceMemoryProperties(this->owner_->physical_device,
			&this->owner_->memory_properties);
	}

	void DeviceResources::create_logical_device()
	{
		const float priority = 1.0f;

		// ONE QUEUE, AND THE SEAM IS WHY. The parallelism axis is views and a
		// view's recording is memory (backend.h), so nothing here ever submits
		// from two threads - which means a second queue would be a second thing
		// to synchronise for no work it could do in parallel (T1).
		VkDeviceQueueCreateInfo queue = {};
		queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue.queueFamilyIndex = this->owner_->queue_family;
		queue.queueCount = 1;
		queue.pQueuePriorities = &priority;

		const char* const extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkPhysicalDeviceVulkan12Features twelve = {};
		twelve.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		twelve.timelineSemaphore = VK_TRUE;

		// ASKED FOR EXPLICITLY, THOUGH IT IS CORE. A 1.2 feature that is core
		// still has to be enabled at device creation to be used, which is the
		// commonest way a first Vulkan port finds vkWaitSemaphores refusing a
		// semaphore it just created.
		VkPhysicalDeviceFeatures2 features = {};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &twelve;
		features.features.textureCompressionBC =
			this->owner_->block_compression ? VK_TRUE : VK_FALSE;

		VkDeviceCreateInfo description = {};
		description.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		description.pNext = &features;
		description.queueCreateInfoCount = 1;
		description.pQueueCreateInfos = &queue;
		description.enabledExtensionCount = 1;
		description.ppEnabledExtensionNames = extensions;

		check_vk(vkCreateDevice(this->owner_->physical_device, &description,
			nullptr, &this->owner_->device),
			"This Vulkan device would not be created");

		vkGetDeviceQueue(this->owner_->device, this->owner_->queue_family, 0,
			&this->owner_->queue);
	}

	void DeviceResources::create_frames()
	{
		VkDevice device = this->owner_->device;

		VkCommandPoolCreateInfo pool = {};
		pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool.queueFamilyIndex = this->owner_->queue_family;

		// NO RESET_COMMAND_BUFFER FLAG, AND THAT IS THE POINT OF A POOL PER
		// FRAME. The whole pool is reset at the top of the frame, which is one
		// call and lets the driver reuse the block; the per-buffer reset flag
		// asks it to track buffers individually instead, and buys nothing here
		// because nothing here ever resets one buffer on its own.
		for (int i = 0; i < FRAME_COUNT; i++)
		{
			Frame& frame = this->frames_[static_cast<size_t>(i)];

			check_vk(vkCreateCommandPool(device, &pool, nullptr,
				&frame.command_pool),
				"A frame's command pool could not be created");

			VkCommandBufferAllocateInfo allocation = {};
			allocation.sType =
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocation.commandPool = frame.command_pool;
			allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocation.commandBufferCount = 1;

			check_vk(vkAllocateCommandBuffers(device, &allocation,
				&frame.commands),
				"A frame's command buffer could not be allocated");

			VkSemaphoreCreateInfo semaphore = {};
			semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			check_vk(vkCreateSemaphore(device, &semaphore, nullptr,
				&frame.acquired),
				"A frame's acquire semaphore could not be created");

			frame.timeline_value = 0;
		}

		VkCommandPoolCreateInfo upload = {};
		upload.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		upload.queueFamilyIndex = this->owner_->queue_family;
		upload.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		check_vk(vkCreateCommandPool(device, &upload, nullptr,
			&this->upload_pool_),
			"The upload command pool could not be created");
	}

	void DeviceResources::destroy_frames()
	{
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE)
		{
			return;
		}

		VkDevice device = this->owner_->device;

		for (int i = 0; i < FRAME_COUNT; i++)
		{
			Frame& frame = this->frames_[static_cast<size_t>(i)];

			for (VkDescriptorPool descriptor_pool : frame.descriptor_pools)
			{
				vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
			}
			frame.descriptor_pools.clear();
			frame.descriptor_pool_in_use = 0;
			frame.descriptor_sets_taken = 0;

			destroy_vulkan_buffer(device, frame.vertices);
			destroy_vulkan_buffer(device, frame.transforms);

			if (frame.acquired != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, frame.acquired, nullptr);
				frame.acquired = VK_NULL_HANDLE;
			}
			if (frame.command_pool != VK_NULL_HANDLE)
			{
				// Frees the command buffer with it, which is why the buffer is
				// not freed on its own.
				vkDestroyCommandPool(device, frame.command_pool, nullptr);
				frame.command_pool = VK_NULL_HANDLE;
				frame.commands = VK_NULL_HANDLE;
			}
			frame.timeline_value = 0;
		}

		this->recording_ = false;
		this->submitted_ = false;
	}

	void DeviceResources::create_render_pass()
	{
		// ONE ATTACHMENT, ONE SUBPASS, AND loadOp IS LOAD RATHER THAN CLEAR.
		//
		// The obvious shape is a render pass that clears, which is what every
		// tutorial writes and what this had first. It does not fit the seam:
		// begin_frame clears and submit() draws, and a client may call the
		// first without ever reaching the second - a frame begun and never
		// submitted (renderer.h) still cleared. A render pass that clears would
		// have to be begun in begin_frame and stay open across every call the
		// caller makes in between, including the ones that are allowed to
		// throw.
		//
		// So the clear is a vkCmdClearColorImage outside any render pass
		// (renderer.cpp, open_frame) and this pass loads what it finds. It
		// costs one image barrier per frame and buys a render pass whose whole
		// life is inside submit().
		VkAttachmentDescription colour = {};
		colour.format = this->colour_format_;
		colour.samples = VK_SAMPLE_COUNT_1_BIT;
		colour.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colour.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colour.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colour.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colour.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colour.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference reference = {};
		reference.attachment = 0;
		reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &reference;

		// The clear that came before this pass, and the blit or the read-back
		// that comes after it. Both are also covered by the explicit barriers
		// transition_colour records; these are what make the pass correct on
		// its own terms rather than on its caller's.
		VkSubpassDependency dependencies[2] = {};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dstStageMask =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		VkRenderPassCreateInfo description = {};
		description.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		description.attachmentCount = 1;
		description.pAttachments = &colour;
		description.subpassCount = 1;
		description.pSubpasses = &subpass;
		description.dependencyCount = 2;
		description.pDependencies = dependencies;

		check_vk(vkCreateRenderPass(this->owner_->device, &description, nullptr,
			&this->render_pass_),
			"The sprite render pass could not be created");
	}

	void DeviceResources::create_device_resources()
	{
		if (this->window_ == nullptr)
		{
			throw std::logic_error(
				"Call set_window with a valid Win32 window handle");
		}

		this->owner_ = std::make_shared<VulkanDevice>();

		this->create_instance();

		VkWin32SurfaceCreateInfoKHR surface = {};
		surface.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface.hinstance = GetModuleHandleW(nullptr);
		surface.hwnd = this->window_;
		check_vk(vkCreateWin32SurfaceKHR(this->owner_->instance, &surface,
			nullptr, &this->surface_),
			"This window could not be given a Vulkan surface");

		this->select_physical_device();
		this->create_logical_device();

		// The timeline, which is this backend's whole synchronisation model.
		VkSemaphoreTypeCreateInfo type = {};
		type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		type.initialValue = 0;

		VkSemaphoreCreateInfo timeline = {};
		timeline.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		timeline.pNext = &type;

		check_vk(vkCreateSemaphore(this->owner_->device, &timeline, nullptr,
			&this->timeline_),
			"The frame timeline could not be created");
		this->timeline_value_ = 0;

		this->create_frames();
		this->create_render_pass();
	}

	void DeviceResources::create_colour_target()
	{
		VkDevice device = this->owner_->device;

		const uint32_t width = std::max<uint32_t>(static_cast<uint32_t>(
			this->output_size_.right - this->output_size_.left), 1u);
		const uint32_t height = std::max<uint32_t>(static_cast<uint32_t>(
			this->output_size_.bottom - this->output_size_.top), 1u);

		this->colour_extent_ = { width, height };

		// EXACTLY THE SIZE THE SHELL SAID, WHICH A SWAPCHAIN IMAGE CANNOT BE.
		// renderer.h's back_buffer_size makes the size of the buffer a term of
		// the seam and says both Direct3D backends answer the size they were
		// told; this is how this backend answers the same, on an API whose
		// surface reports minImageExtent == maxImageExtent == the window.
		VkImageCreateInfo description = {};
		description.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		description.imageType = VK_IMAGE_TYPE_2D;
		description.format = this->colour_format_;
		description.extent = { width, height, 1 };
		description.mipLevels = 1;
		description.arrayLayers = 1;
		description.samples = VK_SAMPLE_COUNT_1_BIT;
		description.tiling = VK_IMAGE_TILING_OPTIMAL;
		description.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		description.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		check_vk(vkCreateImage(device, &description, nullptr,
			&this->colour_image_),
			"The colour target could not be created");

		VkMemoryRequirements requirements = {};
		vkGetImageMemoryRequirements(device, this->colour_image_,
			&requirements);

		VkMemoryAllocateInfo allocation = {};
		allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocation.allocationSize = requirements.size;
		allocation.memoryTypeIndex = this->owner_->memory_type(
			requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "The colour target");

		check_vk(vkAllocateMemory(device, &allocation, nullptr,
			&this->colour_memory_),
			"The colour target's memory could not be allocated");
		check_vk(vkBindImageMemory(device, this->colour_image_,
			this->colour_memory_, 0),
			"The colour target's memory could not be bound");

		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image = this->colour_image_;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = this->colour_format_;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.layerCount = 1;

		check_vk(vkCreateImageView(device, &view, nullptr, &this->colour_view_),
			"The colour target's view could not be created");

		VkFramebufferCreateInfo framebuffer = {};
		framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer.renderPass = this->render_pass_;
		framebuffer.attachmentCount = 1;
		framebuffer.pAttachments = &this->colour_view_;
		framebuffer.width = width;
		framebuffer.height = height;
		framebuffer.layers = 1;

		check_vk(vkCreateFramebuffer(device, &framebuffer, nullptr,
			&this->framebuffer_),
			"The colour target's framebuffer could not be created");

		// A fresh image is in no layout at all, which is what UNDEFINED means -
		// and the first barrier out of it is free, because the contents it
		// discards are contents nothing has written.
		this->colour_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	void DeviceResources::destroy_colour_target()
	{
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE)
		{
			return;
		}

		VkDevice device = this->owner_->device;

		if (this->framebuffer_ != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(device, this->framebuffer_, nullptr);
			this->framebuffer_ = VK_NULL_HANDLE;
		}
		if (this->colour_view_ != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, this->colour_view_, nullptr);
			this->colour_view_ = VK_NULL_HANDLE;
		}
		if (this->colour_image_ != VK_NULL_HANDLE)
		{
			vkDestroyImage(device, this->colour_image_, nullptr);
			this->colour_image_ = VK_NULL_HANDLE;
		}
		if (this->colour_memory_ != VK_NULL_HANDLE)
		{
			vkFreeMemory(device, this->colour_memory_, nullptr);
			this->colour_memory_ = VK_NULL_HANDLE;
		}

		this->colour_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
		this->colour_extent_ = { 0, 0 };
	}

	void DeviceResources::create_swapchain()
	{
		VkDevice device = this->owner_->device;

		VkSurfaceCapabilitiesKHR capabilities = {};
		check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			this->owner_->physical_device, this->surface_, &capabilities),
			"This window's Vulkan surface could not be described");

		if ((capabilities.supportedUsageFlags &
			VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0u)
		{
			throw std::runtime_error("This window's Vulkan surface will not "
				"take a transfer, and this backend presents by blitting its "
				"own colour target into a swapchain image rather than drawing "
				"into one (device_resources.h says why).");
		}

		// WHAT THE SURFACE SAYS, NOT WHAT THE SHELL SAID. On Win32 the two
		// agree whenever the shell is keeping up, and where they do not the
		// surface is the only legal answer: minImageExtent and maxImageExtent
		// are both the window's client area, so a swapchain of any other size
		// is a validation error rather than a stretch. What the SEAM promises
		// is answered by the colour target above, which is why the two are
		// separate objects at all.
		VkExtent2D extent = capabilities.currentExtent;
		if (extent.width == 0xFFFFFFFFu)
		{
			extent.width = std::clamp(this->colour_extent_.width,
				capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width);
			extent.height = std::clamp(this->colour_extent_.height,
				capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height);
		}

		if (extent.width == 0u || extent.height == 0u)
		{
			// A minimised window. There is nothing to present into and there
			// will be again; present() answers this by doing nothing rather
			// than by refusing, because a shell that keeps drawing while
			// minimised is the ordinary case and not a mistake.
			this->swapchain_extent_ = extent;
			return;
		}

		uint32_t formats = 0;
		check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(
			this->owner_->physical_device, this->surface_, &formats, nullptr),
			"This window's Vulkan surface formats could not be listed");
		std::vector<VkSurfaceFormatKHR> surface_formats(formats);
		check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(
			this->owner_->physical_device, this->surface_, &formats,
			surface_formats.data()),
			"This window's Vulkan surface formats could not be listed");

		// B8G8R8A8_UNORM if it is offered, which on Win32 it always is, and the
		// first thing offered otherwise.
		//
		// NOT THE _SRGB ONE, and the difference is the whole pixel contract. An
		// sRGB swapchain format makes the presentation engine convert what it
		// is handed, so the same bytes would come out lighter here than on the
		// four backends whose buffers are UNORM - and read_back_buffer would
		// still hand back the unconverted colour target, so nothing in
		// RenderPixelTests could see it. It would be visible only on screen,
		// only next to another backend.
		VkSurfaceFormatKHR chosen = surface_formats.empty()
			? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM,
				VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
			: surface_formats[0];
		for (const VkSurfaceFormatKHR& candidate : surface_formats)
		{
			if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
				candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				chosen = candidate;
				break;
			}
		}

		uint32_t images = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0u &&
			images > capabilities.maxImageCount)
		{
			images = capabilities.maxImageCount;
		}

		VkSwapchainKHR previous = this->swapchain_;

		VkSwapchainCreateInfoKHR description = {};
		description.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		description.surface = this->surface_;
		description.minImageCount = images;
		description.imageFormat = chosen.format;
		description.imageColorSpace = chosen.colorSpace;
		description.imageExtent = extent;
		description.imageArrayLayers = 1;
		description.imageUsage = SWAPCHAIN_USAGE;
		description.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		description.preTransform = capabilities.currentTransform;
		description.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		// FIFO, WHICH IS THE ONLY MODE EVERY IMPLEMENTATION MUST HAVE and is
		// vsync-locked - the same answer both Direct3D backends give by
		// presenting with a sync interval of 1. Nothing in this tree sets a
		// tearing flag, so there is no capability probe here to measure one.
		description.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		description.clipped = VK_TRUE;
		description.oldSwapchain = previous;

		VkSwapchainKHR fresh = VK_NULL_HANDLE;
		const VkResult created = vkCreateSwapchainKHR(device, &description,
			nullptr, &fresh);

		// The old one is retired whether or not the new one was made: it was
		// handed to oldSwapchain, which puts it into a state where the only
		// legal thing left is to destroy it.
		this->destroy_swapchain();
		check_vk(created, "The swapchain could not be created");

		this->swapchain_ = fresh;
		this->swapchain_format_ = chosen.format;
		this->swapchain_extent_ = extent;

		uint32_t count = 0;
		check_vk(vkGetSwapchainImagesKHR(device, this->swapchain_, &count,
			nullptr), "The swapchain's images could not be listed");
		this->swapchain_images_.resize(count);
		check_vk(vkGetSwapchainImagesKHR(device, this->swapchain_, &count,
			this->swapchain_images_.data()),
			"The swapchain's images could not be listed");

		// ONE PER IMAGE, NOT ONE PER FRAME IN FLIGHT. A semaphore a present is
		// waiting on may not be reused until that present has finished with it,
		// and the presentation engine - not this file - decides which image
		// comes back when. Indexing by the acquired image is the only version
		// of this that is correct rather than usually correct.
		this->presentable_.resize(count, VK_NULL_HANDLE);
		VkSemaphoreCreateInfo semaphore = {};
		semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		for (size_t i = 0; i < this->presentable_.size(); i++)
		{
			check_vk(vkCreateSemaphore(device, &semaphore, nullptr,
				&this->presentable_[i]),
				"A present semaphore could not be created");
		}
	}

	void DeviceResources::rebuild_swapchain()
	{
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE)
		{
			return;
		}

		// A full device wait rather than the timeline one, and it has to be: a
		// present is queued work the timeline does not cover, so a swapchain
		// image the presentation engine is still reading is not something
		// wait_for_gpu can see.
		std::ignore = vkDeviceWaitIdle(this->owner_->device);
		this->create_swapchain();
	}

	void DeviceResources::destroy_swapchain()
	{
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE)
		{
			return;
		}

		VkDevice device = this->owner_->device;

		for (VkSemaphore semaphore : this->presentable_)
		{
			if (semaphore != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, semaphore, nullptr);
			}
		}
		this->presentable_.clear();
		this->swapchain_images_.clear();

		if (this->swapchain_ != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, this->swapchain_, nullptr);
			this->swapchain_ = VK_NULL_HANDLE;
		}
	}

	void DeviceResources::create_window_size_dependent_resources()
	{
		// Nothing may be released while the GPU is still reading it, and the
		// colour target is the thing most likely to be.
		//
		// AND IT ANSWERS RATHER THAN THROWS, for the reason
		// engine/render/d3d12/device_resources.cpp gives at the same place: the
		// device-lost branch is a few lines below, this wait is the first thing
		// to touch the device after a removal, and a throw here unwinds out of
		// a window procedure into DispatchMessage where the shell has nowhere
		// to catch it.
		if (!this->try_wait_for_gpu())
		{
			this->handle_device_lost();
			return;
		}

		if (this->owner_ && this->owner_->device != VK_NULL_HANDLE)
		{
			// Stronger than the timeline wait above, and it has to be: a
			// present is queued work the timeline does not cover, and a
			// swapchain image it is still reading cannot be destroyed.
			std::ignore = vkDeviceWaitIdle(this->owner_->device);
		}

		this->abandon_commands();
		this->destroy_colour_target();
		this->create_colour_target();
		this->create_swapchain();
	}

	bool DeviceResources::window_size_changed(int width, int height)
	{
		if (this->window_ == nullptr)
		{
			return false;
		}

		if (static_cast<long>(width) == this->output_size_.right &&
			static_cast<long>(height) == this->output_size_.bottom)
		{
			return false;
		}

		this->output_size_.left = 0;
		this->output_size_.top = 0;
		this->output_size_.right = static_cast<long>(width);
		this->output_size_.bottom = static_cast<long>(height);

		this->create_window_size_dependent_resources();
		return true;
	}

	void DeviceResources::handle_device_lost()
	{
		if (this->notify_ != nullptr)
		{
			// FIRST, AND IT IS WHAT KEEPS THE OLD DEVICE ALIVE JUST LONG
			// ENOUGH. The shell answers this by emptying the texture table
			// (render_resources.h, release_device_resources), and every texture
			// in it holds a shared_ptr to the VulkanDevice being replaced - so
			// the images are destroyed against the device that made them, in
			// the only window where that is still possible.
			this->notify_->on_device_lost();
		}

		if (this->owner_ && this->owner_->device != VK_NULL_HANDLE)
		{
			std::ignore = vkDeviceWaitIdle(this->owner_->device);
		}

		this->destroy_swapchain();
		this->destroy_colour_target();
		this->destroy_frames();

		if (this->owner_)
		{
			VkDevice device = this->owner_->device;

			if (this->render_pass_ != VK_NULL_HANDLE)
			{
				vkDestroyRenderPass(device, this->render_pass_, nullptr);
				this->render_pass_ = VK_NULL_HANDLE;
			}
			if (this->upload_pool_ != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(device, this->upload_pool_, nullptr);
				this->upload_pool_ = VK_NULL_HANDLE;
			}
			if (this->timeline_ != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, this->timeline_, nullptr);
				this->timeline_ = VK_NULL_HANDLE;
			}
			if (this->surface_ != VK_NULL_HANDLE)
			{
				vkDestroySurfaceKHR(this->owner_->instance, this->surface_,
					nullptr);
				this->surface_ = VK_NULL_HANDLE;
			}
		}

		// Released rather than destroyed: anything still holding one of these -
		// a texture whose owner has not got round to dropping it - keeps the
		// old device alive until it does, which is exactly the guarantee COM
		// gives the D3D12 backend for free.
		this->owner_.reset();

		this->frame_index_ = 0;
		this->timeline_value_ = 0;

		this->create_device_resources();
		this->create_window_size_dependent_resources();

		if (this->notify_ != nullptr)
		{
			this->notify_->on_device_restored();
		}
	}

	// --- The frame's command buffer ------------------------------------------

	VkCommandBuffer DeviceResources::commands()
	{
		Frame& frame = this->frame();

		if (!this->recording_ && this->submitted_)
		{
			// A SECOND OPENING IN ONE FRAME, WHICH IS NOT A BEGIN. The buffer
			// has been to the queue already and is not in the state
			// vkBeginCommandBuffer takes; only a pool reset puts it back there,
			// and a pool may not be reset while anything from it is still
			// executing. The wait is on the value this frame's own submit
			// recorded, so it is exactly as long as it has to be - and it is
			// usually free, because the one caller that gets here is
			// read_back_buffer's, which has already waited for the whole queue.
			this->wait_for_frame();
			std::ignore = vkResetCommandPool(this->owner_->device,
				frame.command_pool, 0);
			this->submitted_ = false;
		}

		if (!this->recording_)
		{
			VkCommandBufferBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			check_vk(vkBeginCommandBuffer(frame.commands, &begin),
				"The frame's command buffer could not be begun");
			this->recording_ = true;
		}

		return frame.commands;
	}

	bool DeviceResources::execute(VkSemaphore wait,
		VkPipelineStageFlags wait_stage, VkSemaphore signal)
	{
		if (!this->recording_)
		{
			return true;
		}

		Frame& frame = this->frame();

		check_vk(vkEndCommandBuffer(frame.commands),
			"The frame's command buffer could not be ended");
		this->recording_ = false;

		this->timeline_value_++;

		// THE TIMELINE ALWAYS, THE BINARY ONE ONLY WHEN A PRESENT IS COMING.
		// Everything the CPU waits for is a value of the first; the second
		// exists because vkQueuePresentKHR takes binary semaphores and nothing
		// else.
		VkSemaphore signals[2] = { this->timeline_, signal };
		uint64_t signal_values[2] = { this->timeline_value_, 0 };
		const uint64_t wait_value = 0;

		VkTimelineSemaphoreSubmitInfo timeline = {};
		timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		timeline.waitSemaphoreValueCount =
			wait != VK_NULL_HANDLE ? 1u : 0u;
		timeline.pWaitSemaphoreValues = &wait_value;
		timeline.signalSemaphoreValueCount =
			signal != VK_NULL_HANDLE ? 2u : 1u;
		timeline.pSignalSemaphoreValues = signal_values;

		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.pNext = &timeline;
		submit.waitSemaphoreCount = wait != VK_NULL_HANDLE ? 1u : 0u;
		submit.pWaitSemaphores = &wait;
		submit.pWaitDstStageMask = &wait_stage;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &frame.commands;
		submit.signalSemaphoreCount = signal != VK_NULL_HANDLE ? 2u : 1u;
		submit.pSignalSemaphores = signals;

		const VkResult submitted = vkQueueSubmit(this->owner_->queue, 1,
			&submit, VK_NULL_HANDLE);

		if (submitted == VK_ERROR_DEVICE_LOST)
		{
			this->handle_device_lost();
			return false;
		}
		check_vk(submitted, "The frame could not be submitted");

		this->submitted_ = true;

		// WHICH FRAME'S WORK THIS WAS, RECORDED FOR THE PASS THAT REUSES IT.
		// Written after every submit rather than only at a present, for the
		// reason engine/render/d3d12/device_resources.h gives on signal_frame:
		// a client that draws, submits and never presents is not hypothetical,
		// and under signal-at-present it would reset a command pool the GPU was
		// still reading.
		frame.timeline_value = this->timeline_value_;
		return true;
	}

	void DeviceResources::abandon_commands()
	{
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE)
		{
			return;
		}

		Frame& frame = this->frame();
		if (frame.command_pool == VK_NULL_HANDLE)
		{
			return;
		}

		// RESET, NOT EXECUTED, AND WHOEVER CALLS THIS HAS ALREADY WAITED. A
		// command pool may not be reset while anything allocated from it is
		// still executing: begin_frame has run wait_for_frame, and the resize
		// path a full device wait. There is no third caller.
		std::ignore = vkResetCommandPool(this->owner_->device,
			frame.command_pool, 0);
		this->recording_ = false;
		this->submitted_ = false;

		// The descriptor sets this frame allocated go with what recorded them.
		// Resetting a pool frees every set in it at once, which is the whole
		// reason they are allocated per run and never tracked individually.
		for (VkDescriptorPool pool : frame.descriptor_pools)
		{
			std::ignore = vkResetDescriptorPool(this->owner_->device, pool, 0);
		}
		frame.descriptor_pool_in_use = 0;
		frame.descriptor_sets_taken = 0;

		// AND THE LAYOUT TRACKING IS FORGOTTEN, WHICH IS NOT TIDINESS. The
		// barriers that moved the colour target into a layout were IN the
		// commands this just threw away, so the member saying where it is has
		// stopped being true - it describes a transition that will never
		// execute. UNDEFINED is the honest word for that: not a layout the
		// image is in, but the statement that nothing here knows, which is
		// exactly what a barrier out of UNDEFINED means and why one is always
		// legal.
		//
		// FOUND BY THE VALIDATION LAYERS AND BY NOTHING ELSE, on
		// tests/render/pixel_tests.cpp's "a frame that is never submitted
		// contributes nothing" - a frame whose clear is recorded and dropped,
		// followed by one that begins a render pass expecting the layout the
		// dropped clear would have left. Every assertion in that case passed
		// while it was wrong, on this machine's driver, because the render pass
		// loads an attachment the clear had just filled either way.
		//
		// THE D3D12 BACKEND CANNOT HAVE THIS, AND THE DIFFERENCE IS ONE LINE OF
		// ITS open_frame: it executes the clear immediately, so its
		// back_buffer_state is true the moment it is written. This backend
		// records the whole frame into one command buffer and submits once,
		// which is cheaper and puts the burden here.
		this->colour_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	// --- Presentation ---------------------------------------------------------

	void DeviceResources::present()
	{
		if (this->swapchain_ == VK_NULL_HANDLE)
		{
			// Minimised, or a swapchain that could not be made at this size.
			// Whatever the frame recorded still goes to the GPU, so a client
			// that draws while minimised is doing ordinary work rather than
			// accumulating it.
			this->execute();
			this->frame_index_ = (this->frame_index_ + 1) % FRAME_COUNT;
			return;
		}

		Frame& frame = this->frame();

		uint32_t image = 0;
		VkResult acquired = vkAcquireNextImageKHR(this->owner_->device,
			this->swapchain_, UINT64_MAX, frame.acquired, VK_NULL_HANDLE,
			&image);

		if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// THE NEWS THE SEAM HAS NO DOOR FOR, AND IT NEEDS NONE. A
			// presentation engine can declare its images stale with no window
			// message anywhere near it - docs/port/android.md calls this the
			// term the fifth backend exists to stress - and what it invalidates
			// is the swapchain, which is not what the seam calls the back
			// buffer. So it is answered here, in full, and
			// Renderer::window_size_changed never hears about it.
			this->rebuild_swapchain();
			if (this->swapchain_ == VK_NULL_HANDLE)
			{
				this->execute();
				this->frame_index_ = (this->frame_index_ + 1) % FRAME_COUNT;
				return;
			}

			acquired = vkAcquireNextImageKHR(this->owner_->device,
				this->swapchain_, UINT64_MAX, this->frame().acquired,
				VK_NULL_HANDLE, &image);
		}

		if (acquired == VK_ERROR_DEVICE_LOST)
		{
			this->handle_device_lost();
			return;
		}
		if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
		{
			check_vk(acquired, "No swapchain image could be acquired");
		}

		Frame& current = this->frame();
		VkCommandBuffer commands = this->commands();

		this->transition_colour(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImage target = this->swapchain_images_[static_cast<size_t>(image)];

		VkImageMemoryBarrier to_destination = image_barrier(target,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
			&to_destination);

		VkImageSubresourceLayers layers = {};
		layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		layers.layerCount = 1;

		// A COPY WHEN THE TWO AGREE AND A BLIT WHEN THEY DO NOT, which is the
		// same distinction DXGI_SCALING_STRETCH makes for the Direct3D
		// backends and makes invisibly. They agree whenever the shell is
		// keeping up; the state where they do not is a drag-resize, which
		// renderer.h's back_buffer_size describes and which this backend is now
		// able to be in for the same reason those two are.
		if (this->colour_extent_.width == this->swapchain_extent_.width &&
			this->colour_extent_.height == this->swapchain_extent_.height)
		{
			VkImageCopy copy = {};
			copy.srcSubresource = layers;
			copy.dstSubresource = layers;
			copy.extent = { this->colour_extent_.width,
				this->colour_extent_.height, 1 };

			vkCmdCopyImage(commands, this->colour_image_,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		}
		else
		{
			VkImageBlit blit = {};
			blit.srcSubresource = layers;
			blit.srcOffsets[1] = {
				static_cast<int32_t>(this->colour_extent_.width),
				static_cast<int32_t>(this->colour_extent_.height), 1 };
			blit.dstSubresource = layers;
			blit.dstOffsets[1] = {
				static_cast<int32_t>(this->swapchain_extent_.width),
				static_cast<int32_t>(this->swapchain_extent_.height), 1 };

			vkCmdBlitImage(commands, this->colour_image_,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
				VK_FILTER_LINEAR);
		}

		VkImageMemoryBarrier to_present = image_barrier(target,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		to_present.dstAccessMask = 0;
		vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
			&to_present);

		if (!this->execute(current.acquired, VK_PIPELINE_STAGE_TRANSFER_BIT,
			this->presentable_[static_cast<size_t>(image)]))
		{
			// The device went away inside that submit and everything has been
			// rebuilt, which includes the swapchain the image index below names
			// and the semaphore the present would wait on. There is nothing
			// left of this frame to present.
			return;
		}

		VkPresentInfoKHR presentation = {};
		presentation.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentation.waitSemaphoreCount = 1;
		presentation.pWaitSemaphores =
			&this->presentable_[static_cast<size_t>(image)];
		presentation.swapchainCount = 1;
		presentation.pSwapchains = &this->swapchain_;
		presentation.pImageIndices = &image;

		const VkResult presented = vkQueuePresentKHR(this->owner_->queue,
			&presentation);

		if (presented == VK_ERROR_DEVICE_LOST)
		{
			this->handle_device_lost();
			return;
		}
		if (presented == VK_ERROR_OUT_OF_DATE_KHR ||
			presented == VK_SUBOPTIMAL_KHR)
		{
			// Rebuilt for the next frame rather than for this one, which has
			// already been presented into an image the engine says is stale.
			// One frame at the old size is what a resize costs everywhere else
			// in this tree too.
			this->rebuild_swapchain();
		}
		else
		{
			check_vk(presented, "The frame could not be presented");
		}

		this->frame_index_ = (this->frame_index_ + 1) % FRAME_COUNT;
	}

	// --- The timeline ---------------------------------------------------------

	void DeviceResources::wait_for_frame()
	{
		const uint64_t target = this->frame().timeline_value;
		if (target == 0 || this->timeline_ == VK_NULL_HANDLE)
		{
			return;
		}

		VkSemaphoreWaitInfo wait = {};
		wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		wait.semaphoreCount = 1;
		wait.pSemaphores = &this->timeline_;
		wait.pValues = &target;

		check_vk(vkWaitSemaphores(this->owner_->device, &wait, UINT64_MAX),
			"The wait for this frame's previous pass did not finish");
	}

	void DeviceResources::wait_for_gpu()
	{
		if (this->timeline_ == VK_NULL_HANDLE || this->timeline_value_ == 0)
		{
			return;
		}

		VkSemaphoreWaitInfo wait = {};
		wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		wait.semaphoreCount = 1;
		wait.pSemaphores = &this->timeline_;
		wait.pValues = &this->timeline_value_;

		check_vk(vkWaitSemaphores(this->owner_->device, &wait, UINT64_MAX),
			"The wait for the GPU did not finish");
	}

	bool DeviceResources::try_wait_for_gpu() noexcept
	{
		if (!this->owner_ || this->owner_->device == VK_NULL_HANDLE ||
			this->timeline_ == VK_NULL_HANDLE || this->timeline_value_ == 0)
		{
			return true;
		}

		VkSemaphoreWaitInfo wait = {};
		wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		wait.semaphoreCount = 1;
		wait.pSemaphores = &this->timeline_;
		wait.pValues = &this->timeline_value_;

		return vkWaitSemaphores(this->owner_->device, &wait, UINT64_MAX) ==
			VK_SUCCESS;
	}

	// --- The colour target ----------------------------------------------------

	void DeviceResources::transition_colour(VkImageLayout layout)
	{
		if (this->colour_layout_ == layout)
		{
			return;
		}

		const VkImageMemoryBarrier barrier = image_barrier(this->colour_image_,
			this->colour_layout_, layout);

		vkCmdPipelineBarrier(this->commands(), stage_of(this->colour_layout_),
			stage_of(layout), 0, 0, nullptr, 0, nullptr, 1, &barrier);

		this->colour_layout_ = layout;
	}

	// --- The frame's buffers --------------------------------------------------

	void DeviceResources::reserve_vertices(VkDeviceSize bytes)
	{
		Frame& frame = this->frame();
		if (bytes == 0 || frame.vertices.bytes >= bytes)
		{
			return;
		}

		// GROWN, NOT WRAPPED, and doubled so that a frame that grows once does
		// not grow again on the next sprite. The D3D12 backend reaches the same
		// answer with pages and the GL one by letting its vertex store grow
		// without a cap; what none of the three does is overwrite memory a
		// draw call already recorded points at.
		//
		// SAFE TO DESTROY THE OLD ONE HERE, which is the only reason this can
		// be a resize rather than a pool: the caller is submit(), which is
		// single-threaded and past wait_for_frame, so nothing the GPU is
		// reading lives in this frame's buffer.
		VkDeviceSize wanted = frame.vertices.bytes > 0
			? frame.vertices.bytes : 64u * 1024u;
		while (wanted < bytes)
		{
			wanted *= 2u;
		}

		destroy_vulkan_buffer(this->owner_->device, frame.vertices);
		frame.vertices = create_vulkan_buffer(*this->owner_, wanted,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			"The frame's vertex buffer");
	}

	void DeviceResources::reserve_transforms(VkDeviceSize bytes)
	{
		Frame& frame = this->frame();
		if (bytes == 0 || frame.transforms.bytes >= bytes)
		{
			return;
		}

		VkDeviceSize wanted = frame.transforms.bytes > 0
			? frame.transforms.bytes : 4u * 1024u;
		while (wanted < bytes)
		{
			wanted *= 2u;
		}

		destroy_vulkan_buffer(this->owner_->device, frame.transforms);
		frame.transforms = create_vulkan_buffer(*this->owner_, wanted,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			"The frame's transform buffer");
	}

	VkDescriptorSet DeviceResources::allocate_descriptor_set(
		VkDescriptorSetLayout layout)
	{
		Frame& frame = this->frame();

		// A FULL POOL IS COUNTED, NOT DISCOVERED. Every set this asks for has
		// the same layout - one uniform buffer, one image, one sampler - so how
		// many a pool holds is a number this file chose, and the pool is done
		// when that many have come out of it. Asking and reacting to
		// VK_ERROR_OUT_OF_POOL_MEMORY is the other way to write this and is
		// worse in the direction that matters: a driver refusing for any other
		// reason turns the retry into a spin.
		if (frame.descriptor_sets_taken >= SETS_PER_POOL &&
			!frame.descriptor_pools.empty())
		{
			frame.descriptor_pool_in_use++;
			frame.descriptor_sets_taken = 0;
		}

		// TAKEN ONCE AND KEPT FOR THE LIFE OF THE PROCESS, because the frame
		// that needed a sixth pool will need one again. Resetting a pool is
		// what makes its sets free; destroying it would be paying for the
		// discovery every frame.
		if (frame.descriptor_pool_in_use >= frame.descriptor_pools.size())
		{
			const VkDescriptorPoolSize sizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SETS_PER_POOL },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SETS_PER_POOL },
				{ VK_DESCRIPTOR_TYPE_SAMPLER, SETS_PER_POOL },
			};

			VkDescriptorPoolCreateInfo description = {};
			description.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			description.maxSets = SETS_PER_POOL;
			description.poolSizeCount = 3;
			description.pPoolSizes = sizes;

			VkDescriptorPool fresh = VK_NULL_HANDLE;
			check_vk(vkCreateDescriptorPool(this->owner_->device,
				&description, nullptr, &fresh),
				"A frame's descriptor pool could not be created");
			frame.descriptor_pools.push_back(fresh);
			frame.descriptor_sets_taken = 0;
		}

		VkDescriptorSetAllocateInfo allocation = {};
		allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocation.descriptorPool =
			frame.descriptor_pools[frame.descriptor_pool_in_use];
		allocation.descriptorSetCount = 1;
		allocation.pSetLayouts = &layout;

		VkDescriptorSet set = VK_NULL_HANDLE;
		check_vk(vkAllocateDescriptorSets(this->owner_->device, &allocation,
			&set), "A run's descriptor set could not be allocated");

		frame.descriptor_sets_taken++;
		return set;
	}

	// --- Uploads --------------------------------------------------------------

	VkCommandBuffer DeviceResources::begin_upload()
	{
		VkCommandBufferAllocateInfo allocation = {};
		allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocation.commandPool = this->upload_pool_;
		allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocation.commandBufferCount = 1;

		VkCommandBuffer commands = VK_NULL_HANDLE;
		check_vk(vkAllocateCommandBuffers(this->owner_->device, &allocation,
			&commands), "An upload command buffer could not be allocated");

		VkCommandBufferBeginInfo begin = {};
		begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		check_vk(vkBeginCommandBuffer(commands, &begin),
			"An upload command buffer could not be begun");

		return commands;
	}

	void DeviceResources::end_upload(VkCommandBuffer commands)
	{
		check_vk(vkEndCommandBuffer(commands),
			"An upload command buffer could not be ended");

		this->timeline_value_++;

		VkTimelineSemaphoreSubmitInfo timeline = {};
		timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		timeline.signalSemaphoreValueCount = 1;
		timeline.pSignalSemaphoreValues = &this->timeline_value_;

		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.pNext = &timeline;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &commands;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &this->timeline_;

		const VkResult submitted = vkQueueSubmit(this->owner_->queue, 1,
			&submit, VK_NULL_HANDLE);
		if (submitted != VK_SUCCESS)
		{
			vkFreeCommandBuffers(this->owner_->device, this->upload_pool_, 1,
				&commands);
			check_vk(submitted, "An upload could not be submitted");
		}

		// A FULL STALL PER UPLOAD, AND IT IS THE RIGHT ANSWER HERE RATHER THAN
		// A SHORTCUT - the same sentence engine/render/d3d12/texture_factory.cpp
		// writes about its own. The staging buffer above this call is a local
		// and the GPU has to be done reading it before it goes; the
		// alternative is a pool and a lifetime rule for a path that reads files
		// off a disk.
		this->wait_for_gpu();

		vkFreeCommandBuffers(this->owner_->device, this->upload_pool_, 1,
			&commands);
	}
}

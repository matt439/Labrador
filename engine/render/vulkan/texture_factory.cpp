#include "engine/render/resource_factory.h"

#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/render/vulkan/backend.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace labrador
{
	namespace
	{
		// Read fresh on every call rather than held, for the reason the other
		// backends' copies of this function give: a cached device is a caller
		// obligation nobody stated, and asking is one member read on the load
		// path.
		VkDevice device_of(const Renderer& renderer)
		{
			return renderer.impl()->device_resources.device();
		}

		// For the throw, because a VkResult is not an answer (T6).
		const char* format_name(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::b8g8r8a8_unorm: return "b8g8r8a8_unorm";
			case TextureFormat::b4g4r4a4_unorm: return "b4g4r4a4_unorm";
			case TextureFormat::bc1_unorm:      return "bc1_unorm";
			case TextureFormat::bc2_unorm:      return "bc2_unorm";
			case TextureFormat::bc3_unorm:      return "bc3_unorm";
			case TextureFormat::r8g8b8a8_unorm:
			default:                            return "r8g8b8a8_unorm";
			}
		}

		// The engine's format vocabulary, into this API's.
		//
		// TWO OF THE SIX ARE REFUSED BY NAME HERE, AND THEY ARE REFUSED FOR
		// DIFFERENT REASONS.
		//
		// The block-compressed three are a DEVICE FEATURE. Vulkan makes BC
		// optional - it is universal on desktop and absent from most mobile
		// GPUs, which is the whole of engine/render/texture_format.h's warning
		// about a second backend and the reason docs/port/android.md puts an
		// ETC2 reader on the spine before this one ever runs on a phone. The
		// answer worth having is the one that names the feature and says how
		// much of the content it is, not a texture that fails to create.
		//
		// b4g4r4a4_unorm is a BIT ORDER, and this is the third backend to
		// refuse it. DXGI_FORMAT_B4G4R4A4_UNORM puts blue in the low nibble,
		// which is Vulkan's VK_FORMAT_A4R4G4B4_UNORM_PACK16 read the other way
		// up - a format that arrived in 1.3, is optional there, and is not what
		// VK_FORMAT_B4G4R4A4_UNORM_PACK16 means. No file in either client uses
		// it (texture_format.h counts them), so the honest thing is to say so
		// rather than to write a swizzle nothing exercises. The GL backend's
		// copy of this refusal makes the same argument in the same words.
		VkFormat to_vk_format(TextureFormat format, bool block_compression,
			const std::string& name)
		{
			switch (format)
			{
			case TextureFormat::r8g8b8a8_unorm:
				return VK_FORMAT_R8G8B8A8_UNORM;
			case TextureFormat::b8g8r8a8_unorm:
				return VK_FORMAT_B8G8R8A8_UNORM;
			case TextureFormat::bc1_unorm:
			case TextureFormat::bc2_unorm:
			case TextureFormat::bc3_unorm:
				break;
			case TextureFormat::b4g4r4a4_unorm:
			default:
				throw std::runtime_error("Texture '" + name + "' is " +
					format_name(format) + ", and this backend does not upload "
					"it. Vulkan's nearest packing puts the channels in a "
					"different order, so it is a conversion rather than a "
					"constant and is not written until something needs it.");
			}

			if (!block_compression)
			{
				throw std::runtime_error("Texture '" + name + "' is block "
					"compressed and this Vulkan device has no "
					"textureCompressionBC. Desktop drivers all provide it; most "
					"mobile GPUs do not, and 41 of the 43 .dds this engine "
					"loads are in it, every font atlas included - so a device "
					"without it has no art and no text.");
			}

			switch (format)
			{
			case TextureFormat::bc1_unorm:
				return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
			case TextureFormat::bc2_unorm:
				return VK_FORMAT_BC2_UNORM_BLOCK;
			case TextureFormat::bc3_unorm:
			default:
				return VK_FORMAT_BC3_UNORM_BLOCK;
			}
		}
	}

	// The whole of this backend's share of loading content, and it is the
	// second longest of the five for the reason the longest one gives: this API
	// has no initial-data parameter either.
	//
	// WHAT A TEXTURE COSTS HERE. An image in device-local memory, an allocation
	// to bind to it because this API never pairs the two, a staging buffer in
	// memory the CPU can write, a copy per mip level recorded onto a command
	// buffer, two barriers that say when the copy may start and when it has
	// finished, and a wait - because the staging buffer is a local and the GPU
	// has to be done reading it before it goes. D3D11 takes an array of
	// D3D11_SUBRESOURCE_DATA and is done; GL takes the bytes; this and D3D12
	// do the work themselves.
	//
	// A FULL STALL PER TEXTURE, AND THAT IS THE RIGHT ANSWER HERE RATHER THAN A
	// SHORTCUT. Loading is a synchronous, blocking path on all five
	// (resource_factory.h says why it reads its file that way); only this one
	// and the D3D12 one wait on a GPU inside it. It runs at load and never on
	// the frame path, and the alternative - keeping every staging buffer alive
	// until some later timeline value - is a pool and a lifetime rule for a
	// path that reads files off a disk.
	void add_texture_asset(const Renderer& renderer,
		RenderResources& resources,
		const std::string& name,
		const TextureData& texture)
	{
		// BEFORE ANYTHING ELSE, because the ordering is a rule of the seam
		// (resource_factory.h) and the honest failure is a named throw rather
		// than a null dereference inside the loader.
		if (device_of(renderer) == nullptr)
		{
			throw std::runtime_error("Texture '" + name + "' was loaded before "
				"create_device made a device.");
		}

		Renderer::Impl& impl = *renderer.impl();
		DeviceResources& device_resources = impl.device_resources;
		const VulkanDevice& owner = device_resources.owner();
		VkDevice device = owner.device;

		const VkFormat format = to_vk_format(texture.format,
			owner.block_compression, name);

		const uint32_t levels = static_cast<uint32_t>(texture.levels.size());

		VkImageCreateInfo description = {};
		description.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		description.imageType = VK_IMAGE_TYPE_2D;
		description.format = format;
		description.extent = { static_cast<uint32_t>(texture.width),
			static_cast<uint32_t>(texture.height), 1 };
		description.mipLevels = levels;
		description.arrayLayers = 1;
		description.samples = VK_SAMPLE_COUNT_1_BIT;
		description.tiling = VK_IMAGE_TILING_OPTIMAL;
		description.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT;
		description.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// ASKED BEFORE IT IS MADE, WHICH IS THE ONE THING THIS API OFFERS THAT
		// THE OTHER FOUR DO NOT. vkGetPhysicalDeviceImageFormatProperties
		// answers "would this image be legal" without creating anything, so a
		// format or a size the device will not take is a sentence naming both
		// rather than a creation failure carrying a number.
		VkImageFormatProperties limits = {};
		const VkResult supported = vkGetPhysicalDeviceImageFormatProperties(
			owner.physical_device, format, description.imageType,
			description.tiling, description.usage, 0, &limits);
		if (supported != VK_SUCCESS)
		{
			throw std::runtime_error("Texture '" + name + "' is " +
				format_name(texture.format) + " at " +
				std::to_string(texture.width) + "x" +
				std::to_string(texture.height) +
				", which this device will not take (" +
				vk_result_name(supported) + ").");
		}
		if (levels > limits.maxMipLevels)
		{
			throw std::runtime_error("Texture '" + name + "' has " +
				std::to_string(levels) + " mip levels, and this device takes "
				"at most " + std::to_string(limits.maxMipLevels) + " for " +
				format_name(texture.format) + ".");
		}

		VkImage image = VK_NULL_HANDLE;
		check_vk(vkCreateImage(device, &description, nullptr, &image),
			"A texture image could not be created");

		VkMemoryRequirements requirements = {};
		vkGetImageMemoryRequirements(device, image, &requirements);

		VkMemoryAllocateInfo allocation = {};
		allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocation.allocationSize = requirements.size;

		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;

		// FROM HERE THE IMAGE IS OWNED BY NOBODY UNTIL THE LAST LINE, so every
		// failure between the two has to put it back. There is no ComPtr in
		// this API and no destructor to lean on: the VulkanTexture that will
		// own all three is not constructed until every one of them exists.
		try
		{
			allocation.memoryTypeIndex = owner.memory_type(
				requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				"A texture image");

			check_vk(vkAllocateMemory(device, &allocation, nullptr, &memory),
				"A texture image's memory could not be allocated");
			check_vk(vkBindImageMemory(device, image, memory, 0),
				"A texture image's memory could not be bound");

			// The staging copy, which is the engine's own bytes unchanged.
			// TextureData packs every level contiguously and says what each
			// one's offset is, so this is one memcpy and a copy region per
			// level rather than a row walk - which is where this backend is
			// cheaper than the D3D12 one, whose runtime pads every row to 256
			// bytes and has to be asked to what.
			VulkanBuffer staging = create_vulkan_buffer(owner,
				static_cast<VkDeviceSize>(texture.pixels.size()),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				"A texture's staging copy");

			std::memcpy(staging.mapped, texture.pixels.data(),
				texture.pixels.size());

			std::vector<VkBufferImageCopy> copies;
			copies.reserve(texture.levels.size());
			for (size_t i = 0; i < texture.levels.size(); i++)
			{
				const TextureLevel& level = texture.levels[i];

				VkBufferImageCopy copy = {};
				copy.bufferOffset = static_cast<VkDeviceSize>(level.offset);

				// ZERO MEANS "AS WIDE AS THE IMAGE", and that is exactly what
				// both of this engine's readers produce - tightly packed rows,
				// which for a block-compressed level is rows of 4x4 blocks
				// (texture_data.h). Stating the row length instead would mean
				// converting the engine's stride, which is in bytes, into
				// texels, which for a compressed format is a division by a
				// block size this file would then have to know.
				copy.bufferRowLength = 0;
				copy.bufferImageHeight = 0;
				copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copy.imageSubresource.mipLevel = static_cast<uint32_t>(i);
				copy.imageSubresource.layerCount = 1;
				copy.imageExtent = { static_cast<uint32_t>(level.width),
					static_cast<uint32_t>(level.height), 1 };

				copies.push_back(copy);
			}

			VkCommandBuffer commands = device_resources.begin_upload();

			VkImageMemoryBarrier to_destination = {};
			to_destination.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			to_destination.srcAccessMask = 0;
			to_destination.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			to_destination.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			to_destination.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			to_destination.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			to_destination.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			to_destination.image = image;
			to_destination.subresourceRange.aspectMask =
				VK_IMAGE_ASPECT_COLOR_BIT;
			to_destination.subresourceRange.levelCount = levels;
			to_destination.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
				&to_destination);

			vkCmdCopyBufferToImage(commands, staging.buffer, image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(copies.size()), copies.data());

			VkImageMemoryBarrier to_shader = to_destination;
			to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
				nullptr, 1, &to_shader);

			// Ends, submits and waits, which is what the staging buffer's
			// lifetime demands.
			device_resources.end_upload(commands);
			destroy_vulkan_buffer(device, staging);

			VkImageViewCreateInfo view_description = {};
			view_description.sType =
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view_description.image = image;
			view_description.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view_description.format = format;
			view_description.subresourceRange.aspectMask =
				VK_IMAGE_ASPECT_COLOR_BIT;
			view_description.subresourceRange.levelCount = levels;
			view_description.subresourceRange.layerCount = 1;

			check_vk(vkCreateImageView(device, &view_description, nullptr,
				&view), "A texture's image view could not be created");
		}
		catch (...)
		{
			if (view != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, view, nullptr);
			}
			if (memory != VK_NULL_HANDLE)
			{
				vkFreeMemory(device, memory, nullptr);
			}
			vkDestroyImage(device, image, nullptr);

			// AND NOTHING WENT IN THE TABLE, which is a term
			// renderer_seam_tests.cpp asserts: "a backend that threw after
			// putting a half-built resource in the table would leave the
			// loader's next resolve answering with it."
			throw;
		}

		// EVERY LEVEL IS IN THE VIEW AND ONLY LEVEL ZERO IS EVER SAMPLED, which
		// is the same pair of facts the other backends state: a chain is read
		// and uploaded and never sampled from, because renderer.h settles that
		// beside set_filter and the samplers in renderer.cpp cap maxLod at
		// zero.
		//
		// ADDED LAST, so the table only ever holds a texture that exists. A
		// name that already has one is replaced and the old one destroyed by
		// Registry::add - which on this backend costs exactly the image it
		// frees, there being no descriptor slot to give back
		// (render_resources.cpp says why that is the one line differing from
		// the D3D12 file).
		resources.impl()->add_texture(name,
			std::make_unique<VulkanTexture>(
				device_resources.shared_device(), image, memory, view,
				texture.width, texture.height));
	}
}

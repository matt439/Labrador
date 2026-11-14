#include "engine/render/vulkan/backend.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/renderer_checks.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"

// The shaders, as bytes this build compiled (cmake/compile_shaders.cmake) from
// engine/render/sprite.hlsl - the same source the two Direct3D backends
// compile, through a different compiler into a different intermediate. That
// file says why it is one file and not three, and compile_shaders.cmake says
// what reaching SPIR-V from HLSL cost and which of the two dxc.exe on a Windows
// developer's machine can do it.
#include "engine/render/vulkan/post_identity_pixel_shader.h"
#include "engine/render/vulkan/post_vertex_shader.h"
#include "engine/render/vulkan/sprite_pixel_shader.h"
#include "engine/render/vulkan/sprite_vertex_shader.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace labrador
{
	namespace
	{
		bool names_software(std::string name)
		{
			std::transform(name.begin(), name.end(), name.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});

			return name.find("llvmpipe") != std::string::npos ||
				name.find("lavapipe") != std::string::npos ||
				name.find("swiftshader") != std::string::npos ||
				name.find("software rasterizer") != std::string::npos;
		}

		// The device DeviceResources selected, and the present mode it
		// configured on its swapchain.
		RenderDeviceInfo selected_device(const DeviceResources& resources)
		{
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(resources.owner().physical_device,
				&properties);

			RenderDeviceInfo info;
			info.backend = "vulkan";
			info.present_mode = resources.present_mode_name();
			info.api = "Vulkan " +
				std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "." +
				std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + "." +
				std::to_string(VK_VERSION_PATCH(properties.apiVersion));
			info.device_name = properties.deviceName;
			info.vendor_id = properties.vendorID;
			info.device_id = properties.deviceID;
			info.kind = properties.deviceType ==
				VK_PHYSICAL_DEVICE_TYPE_CPU || names_software(info.device_name)
				? RenderDeviceKind::software : RenderDeviceKind::hardware;
			return info;
		}

		// Pixels to clip space.
		//
		// THE SAME FOUR NUMBERS THE DIRECT3D BACKENDS WRITE, AND THAT IS A
		// CLAIM ABOUT THE VIEWPORT RATHER THAN ABOUT THE SHADER. Vulkan's clip
		// space has y increasing DOWN where D3D's and GL's increase up, so this
		// shader - which is theirs, unchanged - would draw every frame upside
		// down here. What puts it right is a negative viewport height in
		// submit() below, which is core since 1.1 and is exactly the term
		// renderer.h says every backend decides for itself: where a pane sits
		// in the buffer. GL subtracts the pane's bottom edge from the
		// framebuffer height; this one negates the height. Neither reaches
		// engine/render/sprite.hlsl, which is the test that they belong to a
		// backend.
		void fill_transform(float* transform, const RectangleI& pixels)
		{
			const PixelsToClip values = pixels_to_clip(
				static_cast<float>(pixels.width),
				static_cast<float>(pixels.height));
			transform[0] = values.x_scale;
			transform[1] = values.y_scale;
			transform[2] = values.x_offset;
			transform[3] = values.y_offset;
		}

		// post_frame.hlsli's cbuffer, field for field: eleven float4s, 176
		// bytes - past the 128-byte minimum push-constant size, so it is a
		// uniform buffer (docs/effects.md#4.5).
		struct PostFrameConstants
		{
			float output_size[4];
			float scene_size[4];
			float frame_terms[4];
			float material_values[8][4];
		};
		static_assert(sizeof(PostFrameConstants) == 11 * 4 * sizeof(float),
			"The post frame is eleven float4s, as post_frame.hlsli declares");
		static_assert(sizeof(PostFrameConstants::material_values) ==
			sizeof(PostValues::values),
			"PostValues fills the frame's material_values exactly");

		// The post frame's registers under its binding shifts, b+0, t+1 and
		// s+5 (engine/CMakeLists.txt; docs/effects.md#6.3).
		const uint32_t post_frame_binding = 0;
		const uint32_t post_input_0_binding = 1;
		const uint32_t post_point_sampler_binding = 5;
		const uint32_t post_linear_sampler_binding = 6;

		// A shader module from a byte array this build produced.
		//
		// COPIED INTO uint32_t RATHER THAN CAST TO IT. The generated header
		// declares an unsigned char array, whose alignment is one, and
		// VkShaderModuleCreateInfo::pCode is a const uint32_t* the loader may
		// read as words. It costs a few kilobytes twice at device creation and
		// removes the only place in this backend where a reinterpret_cast would
		// have been relying on a compiler's habits.
		VkShaderModule create_shader(VkDevice device,
			const unsigned char* bytes, size_t size, const char* what)
		{
			if (size == 0 || size % sizeof(uint32_t) != 0)
			{
				throw std::runtime_error(std::string(what) + " is " +
					std::to_string(size) + " bytes, which is not a whole "
					"number of SPIR-V words. The build did not produce what "
					"cmake/compile_shaders.cmake says it produces.");
			}

			std::vector<uint32_t> words(size / sizeof(uint32_t));
			std::memcpy(words.data(), bytes, size);

			VkShaderModuleCreateInfo description = {};
			description.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			description.codeSize = size;
			description.pCode = words.data();

			VkShaderModule shader = VK_NULL_HANDLE;
			check_vk(vkCreateShaderModule(device, &description, nullptr,
				&shader), what);
			return shader;
		}
	}

	// --- DrawList::View ------------------------------------------------------

	void DrawList::View::draw(const VulkanTexture& texture,
		const SpriteVertex* corners)
	{
		this->record(&texture, corners);
	}

	// --- DrawList ------------------------------------------------------------

	void DrawList::set_viewport(const Viewport& viewport)
	{
		// Recorded rather than applied. vkCmdSetViewport is a call on a command
		// buffer this thread does not own, and everything already recorded
		// belongs to the old viewport - so the run closes and the new one
		// carries the new pane, which submit() applies in order.
		this->view_->close_run();
		this->view_->viewport = viewport;
	}

	void DrawList::set_camera(const Camera& camera)
	{
		// No flush: the camera is applied to the geometry as each draw is
		// recorded, not by any state a run holds.
		this->view_->camera = camera;
	}

	const Camera& DrawList::camera() const
	{
		return this->view_->camera;
	}

	const Viewport& DrawList::viewport() const
	{
		return this->view_->viewport;
	}

	void DrawList::set_filter(TextureFilter filter)
	{
		if (this->view_->filter == filter)
		{
			return;
		}
		this->view_->close_run();
		this->view_->filter = filter;
	}

	void DrawList::draw_sprite(TextureHandle texture,
		const RectangleI& source,
		const RectangleF& destination,
		const Colour& tint,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth)
	{
		// Orders nothing; engine/render/SEAM.md#6 says why it is a parameter.
		std::ignore = layer_depth;

		const VulkanTexture& device_texture =
			*this->view_->owner->resources->impl()->texture(texture);

		const ResolvedTexture<const VulkanTexture&> resolved{
			device_texture, device_texture.size() };
		record_sprite(*this->view_, resolved, source, destination, tint,
			rotation, origin, flip);
	}

	void DrawList::draw_text(FontHandle font,
		const std::wstring& text,
		const Vector2F& position,
		const Colour& tint,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth)
	{
		std::ignore = layer_depth;

		// The font is the seam's table and its atlas this folder's
		// (engine/render/render_resources.h).
		const RenderResources& resources = *this->view_->owner->resources;
		const Font& the_font = *resources.font(font);
		const VulkanTexture& atlas =
			*resources.impl()->texture(the_font.atlas());

		const ResolvedTexture<const VulkanTexture&> resolved{
			atlas, atlas.size() };
		record_text(*this->view_, the_font, resolved, text, position, tint,
			scale, rotation, origin);
	}

	// --- Renderer::Impl ------------------------------------------------------

	Renderer::Impl::Impl()
	{
		this->device_resources.register_device_notify(this);
	}

	Renderer::Impl::~Impl()
	{
		// BEFORE device_resources GOES, which is why this destructor exists at
		// all. Members are destroyed in reverse declaration order, so
		// device_resources - declared first - is destroyed last, and everything
		// below it here is a handle whose owner it is. Nothing may be freed
		// while the GPU is reading it, and this API tracks none of that for
		// you.
		//
		// THE WAIT IS THE LINE BELOW AND NOT INSIDE
		// destroy_device_dependent_resources, which contains none. It matters
		// because the OTHER caller is handle_device_lost, by way of
		// on_device_lost, and that path does its own wait first - a second one
		// inside would be on a device that has just gone.
		//
		// AND IT ANSWERS RATHER THAN THROWS, for the reason ~DeviceResources
		// gives: this destructor is implicitly noexcept, so a throw out of the
		// wait would be std::terminate on an ordinary exit and the wait would
		// not have happened either way. T6: teardown stays silent.
		std::ignore = this->device_resources.try_wait_for_gpu();
		this->destroy_device_dependent_resources();
	}

	VkSampler Renderer::Impl::sampler(TextureFilter filter) const
	{
		return filter == TextureFilter::linear
			? this->linear_sampler
			: this->point_sampler;
	}

	void Renderer::Impl::create_device_dependent_resources()
	{
		VkDevice device = this->device_resources.device();

		// --- The descriptor set layout ---------------------------------------
		//
		// THREE BINDINGS IN ONE SET, AND THE NUMBERS ARE THE BUILD'S RATHER
		// THAN THIS FILE'S. HLSL has a register space per resource kind, so
		// sprite.hlsl's b0, t0 and s0 would all arrive at set 0, binding 0 -
		// three resources in one slot and a layout that cannot be written. The
		// shifts that spread them to 0, 1 and 2 are dxc flags in
		// cmake/compile_shaders.cmake, which is where that argument lives; what
		// is here is the other end of it.
		//
		// AND THE TRANSFORM IS A UNIFORM BUFFER WHERE D3D12 HAS ROOT CONSTANTS.
		// Push constants are this API's root constants and are what four floats
		// want - but reaching them from HLSL needs a [[vk::push_constant]]
		// annotation on the cbuffer, which is an edit to a file two other
		// backends compile. sprite.hlsl says a backend owns "the profile and
		// how it binds b0" and that neither difference reaches the shader; a
		// push constant would be the first one that did. So the binding pays a
		// uniform buffer and the shared source stays shared.
		VkDescriptorSetLayoutBinding bindings[3] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		bindings[2].descriptorCount = 1;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo set_description = {};
		set_description.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set_description.bindingCount = 3;
		set_description.pBindings = bindings;

		VkDescriptorSetLayout made_set_layout = VK_NULL_HANDLE;
		check_vk(vkCreateDescriptorSetLayout(device, &set_description, nullptr,
			&made_set_layout),
			"The sprite descriptor set layout could not be created");
		this->set_layout = made_set_layout;

		VkPipelineLayoutCreateInfo layout_description = {};
		layout_description.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_description.setLayoutCount = 1;
		layout_description.pSetLayouts = &this->set_layout;

		VkPipelineLayout made_pipeline_layout = VK_NULL_HANDLE;
		check_vk(vkCreatePipelineLayout(device, &layout_description, nullptr,
			&made_pipeline_layout),
			"The sprite pipeline layout could not be created");
		this->pipeline_layout = made_pipeline_layout;

		// --- The pipeline ----------------------------------------------------
		//
		// ONE OBJECT, AS ON D3D12, AND MADE ONCE. The blend, the rasteriser
		// state, the vertex layout and both shaders are fields of it, so there
		// is nothing to bind per draw call that the D3D11 backend binds. The
		// two samplers are the exception and are not in here: the seam lets a
		// draw list change its filter mid-list, so they are descriptors a draw
		// call names rather than state a pipeline fixes.
		VkShaderModule vertex_shader = create_shader(device,
			sprite_vertex_shader, sizeof(sprite_vertex_shader),
			"The sprite vertex shader could not be created");
		VkShaderModule pixel_shader = VK_NULL_HANDLE;
		try
		{
			pixel_shader = create_shader(device,
				sprite_pixel_shader, sizeof(sprite_pixel_shader),
				"The sprite pixel shader could not be created");
		}
		catch (...)
		{
			vkDestroyShaderModule(device, vertex_shader, nullptr);
			throw;
		}

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType =
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertex_shader;
		stages[0].pName = "vertex_main";

		stages[1].sType =
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = pixel_shader;
		stages[1].pName = "pixel_main";

		VkVertexInputBindingDescription binding = {};
		binding.binding = 0;
		binding.stride = static_cast<uint32_t>(sizeof(SpriteVertex));
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// The three fields of SpriteVertex, located by offsetof rather than by
		// position, which is why sprite_vertex.h is free to declare them in any
		// order it likes. The locations are the shader's, assigned by dxc in
		// the order the HLSL struct declares its semantics.
		VkVertexInputAttributeDescription attributes[3] = {};
		attributes[0].location = 0;
		attributes[0].binding = 0;
		attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributes[0].offset =
			static_cast<uint32_t>(offsetof(SpriteVertex, position));

		attributes[1].location = 1;
		attributes[1].binding = 0;
		attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributes[1].offset =
			static_cast<uint32_t>(offsetof(SpriteVertex, colour));

		attributes[2].location = 2;
		attributes[2].binding = 0;
		attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributes[2].offset =
			static_cast<uint32_t>(offsetof(SpriteVertex, texcoord));

		VkPipelineVertexInputStateCreateInfo vertex_input = {};
		vertex_input.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input.vertexBindingDescriptionCount = 1;
		vertex_input.pVertexBindingDescriptions = &binding;
		vertex_input.vertexAttributeDescriptionCount = 3;
		vertex_input.pVertexAttributeDescriptions = attributes;

		VkPipelineInputAssemblyStateCreateInfo assembly = {};
		assembly.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// BOTH DYNAMIC, WHICH IS NOT A PREFERENCE. The seam lets a draw list
		// change its viewport mid-list and a frame fan out to several panes, so
		// a pipeline that baked one would need a pipeline per pane.
		VkPipelineViewportStateCreateInfo viewport_state = {};
		viewport_state.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.scissorCount = 1;

		const VkDynamicState dynamic_states[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynamic = {};
		dynamic.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = dynamic_states;

		// NO CULLING. A sprite is a quad whose winding never changes: a flip
		// mirrors the texture coordinates and leaves the corners where they
		// were (sprite_geometry.cpp). It matters more here than anywhere
		// else, because the negative viewport height in submit() reverses the
		// winding as it reverses y - so a backend that culled would have to
		// match its front-face convention to that viewport.
		VkPipelineRasterizationStateCreateInfo rasteriser = {};
		rasteriser.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
		rasteriser.cullMode = VK_CULL_MODE_NONE;
		rasteriser.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasteriser.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisample = {};
		multisample.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// PREMULTIPLIED ALPHA: the source factor is ONE and not SRC_ALPHA.
		// RenderPixelTests calls this the term most likely to be got wrong,
		// because both answers look plausible and every opaque sprite renders
		// identically either way. Every backend's blend state carries the same
		// two words.
		VkPipelineColorBlendAttachmentState blend = {};
		blend.blendEnable = VK_TRUE;
		blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend.colorBlendOp = VK_BLEND_OP_ADD;
		blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend.alphaBlendOp = VK_BLEND_OP_ADD;
		blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blending = {};
		blending.sType =
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blending.attachmentCount = 1;
		blending.pAttachments = &blend;

		VkGraphicsPipelineCreateInfo description = {};
		description.sType =
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		description.stageCount = 2;
		description.pStages = stages;
		description.pVertexInputState = &vertex_input;
		description.pInputAssemblyState = &assembly;
		description.pViewportState = &viewport_state;
		description.pRasterizationState = &rasteriser;
		description.pMultisampleState = &multisample;

		// No depth state at all, because there is no depth attachment for one
		// to apply to - which this API lets a pipeline say by omission where
		// D3D12 wants DepthEnable = FALSE spelt out.
		description.pDepthStencilState = nullptr;
		description.pColorBlendState = &blending;
		description.pDynamicState = &dynamic;
		description.layout = this->pipeline_layout;
		description.renderPass = this->device_resources.render_pass();
		description.subpass = 0;

		VkPipeline made_pipeline = VK_NULL_HANDLE;
		const VkResult built = vkCreateGraphicsPipelines(device,
			VK_NULL_HANDLE, 1, &description, nullptr, &made_pipeline);

		vkDestroyShaderModule(device, vertex_shader, nullptr);
		vkDestroyShaderModule(device, pixel_shader, nullptr);

		check_vk(built, "The sprite pipeline could not be created");
		this->pipeline = made_pipeline;

		// --- The samplers ----------------------------------------------------
		//
		// Clamped, so a source rectangle at the edge of an atlas cannot bleed
		// the far side of it into a sprite - which is what a sheet is full of.
		//
		// LEVEL ZERO, ALWAYS, and maxLod is where this backend says it: a
		// minified draw samples level zero however many levels the texture
		// carries, under either filter. renderer.h decides that beside
		// set_filter and says why it is the seam's decision and not a
		// backend's.
		VkSamplerCreateInfo sampler_description = {};
		sampler_description.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_description.addressModeU =
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_description.addressModeV =
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_description.addressModeW =
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_description.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler_description.minLod = 0.0f;
		sampler_description.maxLod = 0.0f;
		sampler_description.maxAnisotropy = 1.0f;
		sampler_description.borderColor =
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		sampler_description.magFilter = VK_FILTER_NEAREST;
		sampler_description.minFilter = VK_FILTER_NEAREST;
		VkSampler made_point_sampler = VK_NULL_HANDLE;
		check_vk(vkCreateSampler(device, &sampler_description, nullptr,
			&made_point_sampler),
			"The point sampler could not be created");
		this->point_sampler = made_point_sampler;

		sampler_description.magFilter = VK_FILTER_LINEAR;
		sampler_description.minFilter = VK_FILTER_LINEAR;
		VkSampler made_linear_sampler = VK_NULL_HANDLE;
		check_vk(vkCreateSampler(device, &sampler_description, nullptr,
			&made_linear_sampler),
			"The linear sampler could not be created");
		this->linear_sampler = made_linear_sampler;

		// --- The index buffer -------------------------------------------------
		//
		// The same two triangles per sprite for every sprite there will ever
		// be, so it is filled once and never touched again. IN DEVICE-LOCAL
		// MEMORY AND COPIED THERE, where D3D11 says IMMUTABLE and hands over
		// the bytes: this API has no initial-data parameter either, so
		// "upload it once at creation" is a staging buffer, a copy on a command
		// buffer and a wait - the same three steps every texture takes
		// (texture_factory.cpp).
		const std::vector<unsigned short> index_data =
			quad_indices(DrawList::View::max_run_sprites);

		const VkDeviceSize index_bytes =
			static_cast<VkDeviceSize>(index_data.size()) *
			sizeof(unsigned short);

		this->indices = create_vulkan_buffer(this->device_resources.owner(),
			index_bytes,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "The sprite index buffer");

		// RELEASED ON EVERY WAY OUT, because begin_upload, end_upload and the
		// wait inside it all throw, and this path also runs inside
		// handle_device_lost's restore, where a leak is a leak per loss rather
		// than one per process.
		const ScopedVulkanBuffer staging(this->device_resources.shared_device(),
			index_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			"The sprite index buffer's staging copy");

		std::memcpy(staging.buffer().mapped, index_data.data(),
			static_cast<size_t>(index_bytes));

		VkCommandBuffer upload = this->device_resources.begin_upload();

		VkBufferCopy copy = {};
		copy.size = index_bytes;
		vkCmdCopyBuffer(upload, staging.buffer().buffer, this->indices.buffer,
			1, &copy);

		// TRANSFER_WRITE -> INDEX_READ, WHICH IS THE SAME BARRIER THE TEXTURE
		// PATH EMITS AND FOR THE SAME REASON. end_upload waits for the whole
		// queue afterwards, so this looks redundant - and it is exactly as
		// redundant as texture_factory.cpp's TRANSFER -> FRAGMENT_SHADER
		// barrier, which is there because a host wait is not an availability
		// operation for a later queue access.
		VkBufferMemoryBarrier to_index = {};
		to_index.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		to_index.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_index.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
		to_index.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_index.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_index.buffer = this->indices.buffer;
		to_index.offset = 0;
		to_index.size = VK_WHOLE_SIZE;

		vkCmdPipelineBarrier(upload, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
			&to_index, 0, nullptr);

		this->device_resources.end_upload(upload);

		this->create_post_resources();
	}

	void Renderer::Impl::create_post_resources()
	{
		VkDevice device = this->device_resources.device();

		// THE FRAME'S REGISTERS, NOT A MATERIAL'S. post_frame.hlsli declares
		// every one, so this layout serves every material.
		VkDescriptorSetLayoutBinding bindings[4] = {};
		bindings[0].binding = post_frame_binding;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[1].binding = post_input_0_binding;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[2].binding = post_point_sampler_binding;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		bindings[2].descriptorCount = 1;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[3].binding = post_linear_sampler_binding;
		bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		bindings[3].descriptorCount = 1;
		bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo set_description = {};
		set_description.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set_description.bindingCount = 4;
		set_description.pBindings = bindings;

		VkDescriptorSetLayout made_set_layout = VK_NULL_HANDLE;
		check_vk(vkCreateDescriptorSetLayout(device, &set_description, nullptr,
			&made_set_layout),
			"The post descriptor set layout could not be created");
		this->post_set_layout = made_set_layout;

		VkPipelineLayoutCreateInfo layout_description = {};
		layout_description.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_description.setLayoutCount = 1;
		layout_description.pSetLayouts = &this->post_set_layout;

		VkPipelineLayout made_pipeline_layout = VK_NULL_HANDLE;
		check_vk(vkCreatePipelineLayout(device, &layout_description, nullptr,
			&made_pipeline_layout),
			"The post pipeline layout could not be created");
		this->post_pipeline_layout = made_pipeline_layout;

		VkShaderModule vertex_shader = create_shader(device,
			post_vertex_bytecode, sizeof(post_vertex_bytecode),
			"The post vertex shader could not be created");
		VkShaderModule pixel_shader = VK_NULL_HANDLE;
		try
		{
			pixel_shader = create_shader(device,
				post_identity_bytecode, sizeof(post_identity_bytecode),
				"The identity post shader could not be created");
		}
		catch (...)
		{
			vkDestroyShaderModule(device, vertex_shader, nullptr);
			throw;
		}

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType =
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertex_shader;
		stages[0].pName = "post_vertex_main";

		stages[1].sType =
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = pixel_shader;
		stages[1].pName = "post_pixel_main";

		// NO VERTEX INPUT: the triangle comes from the vertex index alone.
		VkPipelineVertexInputStateCreateInfo vertex_input = {};
		vertex_input.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkPipelineInputAssemblyStateCreateInfo assembly = {};
		assembly.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineViewportStateCreateInfo viewport_state = {};
		viewport_state.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.scissorCount = 1;

		const VkDynamicState dynamic_states[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynamic = {};
		dynamic.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = dynamic_states;

		// No culling, for the sprite pipeline's reason: the negative viewport
		// reverses the winding.
		VkPipelineRasterizationStateCreateInfo rasteriser = {};
		rasteriser.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
		rasteriser.cullMode = VK_CULL_MODE_NONE;
		rasteriser.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasteriser.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisample = {};
		multisample.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// A FULL-SCREEN PASS REPLACES; only draw lists blend
		// (docs/effects.md#5.3).
		VkPipelineColorBlendAttachmentState blend = {};
		blend.blendEnable = VK_FALSE;
		blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blending = {};
		blending.sType =
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blending.attachmentCount = 1;
		blending.pAttachments = &blend;

		// Against the loading pass, with which the output pass is compatible
		// (device_resources.h, output_render_pass).
		VkGraphicsPipelineCreateInfo description = {};
		description.sType =
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		description.stageCount = 2;
		description.pStages = stages;
		description.pVertexInputState = &vertex_input;
		description.pInputAssemblyState = &assembly;
		description.pViewportState = &viewport_state;
		description.pRasterizationState = &rasteriser;
		description.pMultisampleState = &multisample;
		description.pDepthStencilState = nullptr;
		description.pColorBlendState = &blending;
		description.pDynamicState = &dynamic;
		description.layout = this->post_pipeline_layout;
		description.renderPass = this->device_resources.render_pass();
		description.subpass = 0;

		VkPipeline made_pipeline = VK_NULL_HANDLE;
		const VkResult built = vkCreateGraphicsPipelines(device,
			VK_NULL_HANDLE, 1, &description, nullptr, &made_pipeline);

		vkDestroyShaderModule(device, vertex_shader, nullptr);
		vkDestroyShaderModule(device, pixel_shader, nullptr);

		check_vk(built, "The identity post pipeline could not be created");
		this->post_identity_pipeline = made_pipeline;
	}

	void Renderer::Impl::destroy_device_dependent_resources()
	{
		VkDevice device = this->device_resources.device();
		if (device == VK_NULL_HANDLE)
		{
			this->indices = VulkanBuffer{};
			this->pipeline = VK_NULL_HANDLE;
			this->pipeline_layout = VK_NULL_HANDLE;
			this->set_layout = VK_NULL_HANDLE;
			this->point_sampler = VK_NULL_HANDLE;
			this->linear_sampler = VK_NULL_HANDLE;
			this->post_identity_pipeline = VK_NULL_HANDLE;
			this->post_pipeline_layout = VK_NULL_HANDLE;
			this->post_set_layout = VK_NULL_HANDLE;
			return;
		}

		destroy_vulkan_buffer(device, this->indices);

		if (this->pipeline != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device, this->pipeline, nullptr);
			this->pipeline = VK_NULL_HANDLE;
		}
		if (this->pipeline_layout != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(device, this->pipeline_layout, nullptr);
			this->pipeline_layout = VK_NULL_HANDLE;
		}
		if (this->set_layout != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(device, this->set_layout, nullptr);
			this->set_layout = VK_NULL_HANDLE;
		}
		if (this->point_sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, this->point_sampler, nullptr);
			this->point_sampler = VK_NULL_HANDLE;
		}
		if (this->linear_sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, this->linear_sampler, nullptr);
			this->linear_sampler = VK_NULL_HANDLE;
		}
		if (this->post_identity_pipeline != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device, this->post_identity_pipeline, nullptr);
			this->post_identity_pipeline = VK_NULL_HANDLE;
		}
		if (this->post_pipeline_layout != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(device, this->post_pipeline_layout,
				nullptr);
			this->post_pipeline_layout = VK_NULL_HANDLE;
		}
		if (this->post_set_layout != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(device, this->post_set_layout,
				nullptr);
			this->post_set_layout = VK_NULL_HANDLE;
		}
	}

	void Renderer::Impl::reset_views()
	{
		const VkExtent2D extent = this->device_resources.colour_extent();

		for (std::unique_ptr<DrawList::View>& view : this->views)
		{
			view->reset();
			view->viewport = Viewport(0.0f, 0.0f,
				static_cast<float>(extent.width),
				static_cast<float>(extent.height));
		}
	}

	void Renderer::Impl::abandon_recording()
	{
		// Drop the old CPU recording even if open_frame later fails before
		// reaching its reset against the new colour target's extent.
		this->reset_views();
		this->device_resources.abandon_commands();
	}

	void Renderer::Impl::open_frame()
	{
		// THE CLEAR IS A TRANSFER RATHER THAN A RENDER PASS' loadOp, and
		// device_resources.cpp's create_render_pass says why: a pass that
		// cleared would have to be begun here and stay open across every call
		// the caller makes before submit(), including the ones the seam allows
		// to throw. This costs two image barriers a frame and buys a render
		// pass whose whole life is inside submit().
		//
		// WITH A CHAIN, BOTH ARE CLEARED: the scene because the processed
		// views draw into it, and the colour target so that a frame begun and
		// never submitted presents black rather than the last frame's post
		// output (docs/effects.md#4.1).
		const bool scene = this->device_resources.has_scene_target();

		this->device_resources.transition_colour(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		if (scene)
		{
			this->device_resources.transition_scene(
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		}

		// Opaque black, spelt out - the clear colour is a term RenderPixelTests
		// pins, so it is worth being able to read it here.
		VkClearColorValue black = {};
		black.float32[0] = 0.0f;
		black.float32[1] = 0.0f;
		black.float32[2] = 0.0f;
		black.float32[3] = 1.0f;

		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.levelCount = 1;
		range.layerCount = 1;

		vkCmdClearColorImage(this->device_resources.commands(),
			this->device_resources.colour_target(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
		if (scene)
		{
			vkCmdClearColorImage(this->device_resources.commands(),
				this->device_resources.scene_target(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
		}

		this->device_resources.transition_colour(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		if (scene)
		{
			this->device_resources.transition_scene(
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}

		// The views the frame will draw into, against the buffer that now
		// exists. view_count is deliberately not touched: begin_frame sets it
		// to zero before calling this and a resize must not, because the layout
		// is the shell's to decide and the resize is what tells it to decide
		// again (renderer.h, window_size_changed).
		this->reset_views();
	}

	void Renderer::Impl::on_device_lost()
	{
		this->device_info = {};
		this->destroy_device_dependent_resources();

		if (this->notify != nullptr)
		{
			this->notify->on_device_lost();
		}
	}

	void Renderer::Impl::on_device_restored()
	{
		this->create_device_dependent_resources();
		this->device_info = selected_device(this->device_resources);

		if (this->notify != nullptr)
		{
			this->notify->on_device_restored();
		}
	}

	// --- Renderer ------------------------------------------------------------

	Renderer::Renderer() : impl_(std::make_unique<Impl>())
	{

	}

	Renderer::~Renderer() = default;
	Renderer::Renderer(Renderer&&) noexcept = default;
	Renderer& Renderer::operator=(Renderer&&) noexcept = default;

	void Renderer::create_device(void* native_window, int width, int height,
		int view_capacity)
	{
		check_view_capacity(view_capacity);

		Impl& impl = *this->impl_;

		impl.device_resources.set_window(static_cast<HWND>(native_window),
			width, height);
		impl.device_resources.create_device_resources();

		impl.views.clear();
		impl.views.reserve(static_cast<size_t>(view_capacity));
		for (int i = 0; i < view_capacity; i++)
		{
			impl.views.push_back(std::make_unique<DrawList::View>());
			impl.views.back()->owner = &impl;
			impl.views.back()->reset();
		}

		// The pipeline before the colour target, because a pipeline names a
		// render pass and a framebuffer names one too - and the render pass is
		// the device's rather than the window's, being a description of formats
		// and not of sizes.
		impl.create_device_dependent_resources();
		impl.device_resources.create_window_size_dependent_resources();

		impl.reset_views();
		impl.device_info = selected_device(impl.device_resources);
	}

	const RenderDeviceInfo& Renderer::device_info() const
	{
		check_device_created(!this->impl_->device_info.backend.empty());
		return this->impl_->device_info;
	}

	bool Renderer::window_size_changed(int width, int height)
	{
		Impl& impl = *this->impl_;

		// BEFORE THERE IS A DEVICE THERE IS NOTHING TO REBUILD, and the seam
		// says the answer is false (renderer.h). Reached by a shell that gets a
		// WM_SIZE between creating its window and creating its device, which is
		// an ordering Win32 allows and nothing here forbids -
		// tests/render/renderer_seam_tests.cpp holds all five backends to it.
		if (impl.device_resources.device() == VK_NULL_HANDLE)
		{
			return false;
		}

		// ASKED BEFORE ANYTHING IS THROWN AWAY, because most calls to this
		// change nothing and a frame is not worth losing to one of them.
		// Application::on_window_moved calls this with the size it already has
		// on every move of the window outside a drag, and the shell calls it
		// again on WM_EXITSIZEMOVE with a size the drag may well have returned
		// to.
		const RECT size = impl.device_resources.output_size();
		if (static_cast<long>(width) == size.right - size.left &&
			static_cast<long>(height) == size.bottom - size.top)
		{
			return false;
		}

		// A FRAME IN PROGRESS IS RESTARTED, NOT REFUSED, and renderer.h states
		// that as a term of the seam rather than leaving it to a backend.
		//
		// THE VIEWS GO NOW AND THE COMMAND BUFFER GOES LATER, WHICH IS THE ONE
		// PLACE THIS BACKEND CANNOT USE Impl::abandon_recording. A view's
		// recording is memory and dropping it is free at any moment; the frame's
		// command buffer belongs to a pool that may not be reset while anything
		// from it is still executing, and this frame may already have submitted
		// once - a read_back_buffer does exactly that. The wait that makes the
		// reset safe is a full device wait inside
		// DeviceResources::create_window_size_dependent_resources, which then
		// abandons the commands itself, in that order.
		const bool restart = impl.frame_begun;
		impl.reset_views();

		const bool rebuilt =
			impl.device_resources.window_size_changed(width, height);

		if (rebuilt && restart)
		{
			// Cleared and reopened against the buffer that now exists, so a
			// DrawList the caller is still holding draws into this frame
			// instead of into a resource that has gone. With no views declared
			// yet it is the clear alone, which is exactly what the frame is
			// owed at that point.
			impl.open_frame();
		}

		return rebuilt;
	}

	void Renderer::set_device_notify(DeviceNotify* device_notify)
	{
		this->impl_->notify = device_notify;
	}

	void Renderer::set_resources(const RenderResources* resources)
	{
		this->impl_->resources = resources;
	}

	void Renderer::begin_frame()
	{
		Impl& impl = *this->impl_;

		// BEFORE ANYTHING RESETS A COMMAND POOL OR OVERWRITES A VERTEX BUFFER,
		// which is the whole of what this backend adds to the seam's frame -
		// and it is the same line the D3D12 one adds, because the two APIs ask
		// the same thing of the engine. Nothing above the seam knows or needs
		// to.
		impl.device_resources.wait_for_frame();

		// A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT ONE
		// (renderer.h). Here that is a vector per view and a command pool, both
		// of which are safe to reset because of the wait above.
		impl.abandon_recording();

		// Before the clear, so that open_frame opens no views: this frame has
		// not said how many it has. The resize path calls the same function
		// with a count already set, which is the whole difference between
		// starting a frame and restarting one.
		impl.view_count = 0;
		impl.overlay_start = -1;

		// SET BEFORE THE CLEAR AND NOT AFTER IT, so that the frame owns
		// everything open_frame is about to do to the colour target. From here
		// until end_frame, window_size_changed has a frame to restart.
		impl.frame_begun = true;
		impl.frame_submitted = false;

		impl.open_frame();
	}

	void Renderer::end_frame()
	{
		Impl& impl = *this->impl_;

		// Cleared before the present rather than after it, because present() is
		// where a device loss surfaces and what it does about one is rebuild
		// every resource this frame was drawn with. There is nothing left to
		// restart by then.
		impl.frame_begun = false;

		impl.device_resources.present();

		// The mode the swapchain now has, which a set_vsync since the last
		// present has just changed. Empty while a lost device is between its
		// two halves, and filled again by on_device_restored.
		if (!impl.device_info.backend.empty())
		{
			impl.device_info.present_mode =
				impl.device_resources.present_mode_name();
		}
	}

	void Renderer::set_vsync(bool vsync)
	{
		this->impl_->device_resources.set_vsync(vsync);
	}

	bool Renderer::vsync() const
	{
		return this->impl_->device_resources.vsync();
	}

	void Renderer::set_view_count(int count)
	{
		check_view_count(count, this->impl_->view_count,
			this->impl_->views);

		this->impl_->view_count = count;
	}

	int Renderer::view_count() const
	{
		return this->impl_->view_count;
	}

	DrawList Renderer::view(int index) const
	{
		check_view_index(index, this->impl_->view_count);
		return DrawList(this->impl_->views[static_cast<size_t>(index)].get());
	}

	void Renderer::submit()
	{
		// ONCE PER FRAME, AND A SECOND CALL ADDS NOTHING (renderer.h). Cleared
		// by begin_frame, which is the only thing that starts a frame.
		if (this->impl_->frame_submitted)
		{
			return;
		}
		this->impl_->frame_submitted = true;

		Impl& impl = *this->impl_;
		DeviceResources& device_resources = impl.device_resources;

		// EVERYTHING THE GPU WILL SEE IS BUILT HERE, ON ONE THREAD, AND THAT IS
		// THE SHAPE OF THIS BACKEND. A view's recording is memory (backend.h),
		// so nothing before this line has touched a buffer, a descriptor or a
		// command. What that buys is the thing this function opens with: the
		// whole frame's vertex count and run count are known before a single
		// resource is sized, so the buffers are taken once and the descriptor
		// sets come out of a pool that is reset with the frame.
		size_t total_vertices = 0;
		size_t total_runs = 0;

		for (int i = 0; i < impl.view_count; i++)
		{
			DrawList::View& view = *impl.views[static_cast<size_t>(i)];
			view.close_run();
			total_vertices += view.vertices.size();
			total_runs += view.runs.size();
		}

		// WITHOUT A CHAIN, A FRAME WITH NOTHING TO DRAW RECORDS NOTHING: the
		// clear open_frame recorded is the whole frame. With one, the post pass
		// runs anyway, so a frame with no views presents the chain over a
		// black scene rather than whatever the colour target held.
		const bool chain = device_resources.has_scene_target();
		if (total_runs == 0 && !chain)
		{
			return;
		}

		const VkDeviceSize transform_stride =
			device_resources.owner().uniform_alignment;

		// THE POST FRAME RIDES IN THE TRANSFORM BUFFER, after the last run's
		// slot: the stride is a multiple of the device's uniform offset
		// alignment, so the offset is a legal one, and the buffer is already
		// per frame in flight and remade with the device.
		const VkDeviceSize post_frame_offset =
			static_cast<VkDeviceSize>(total_runs) * transform_stride;
		VkDeviceSize post_frame_bytes = 0;
		if (chain)
		{
			const VkDeviceSize constants = sizeof(PostFrameConstants);
			post_frame_bytes = (constants + transform_stride - 1) /
				transform_stride * transform_stride;
		}

		device_resources.reserve_vertices(
			static_cast<VkDeviceSize>(total_vertices) * sizeof(SpriteVertex));
		device_resources.reserve_transforms(
			post_frame_offset + post_frame_bytes);

		DeviceResources::Frame& frame = device_resources.frame();

		// A run in this frame's vertex buffer is addressed by a vertex offset
		// on the draw call, which is what makes one buffer and one binding
		// serve every view - the same origin gl/renderer.cpp moves by
		// re-pointing its vertex attributes at a run's first vertex, spelt
		// here as a parameter of the draw rather than as three calls per run.
		unsigned char* vertex_bytes =
			static_cast<unsigned char*>(frame.vertices.mapped);

		size_t vertex_base = 0;

		for (int i = 0; i < impl.view_count; i++)
		{
			const DrawList::View& view = *impl.views[static_cast<size_t>(i)];

			if (!view.vertices.empty())
			{
				std::memcpy(
					vertex_bytes + vertex_base * sizeof(SpriteVertex),
					view.vertices.data(),
					view.vertices.size() * sizeof(SpriteVertex));
			}

			vertex_base += view.vertices.size();
		}

		VkCommandBuffer commands = device_resources.commands();
		const VkExtent2D extent = device_resources.colour_extent();

		VkRenderPassBeginInfo pass = {};
		pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		pass.renderArea.offset = { 0, 0 };
		pass.renderArea.extent = extent;

		size_t transform_slot = 0;
		vertex_base = 0;

		if (!chain)
		{
			// A no-op in every ordinary frame - open_frame left it a colour
			// attachment. It is here for the frame that reaches submit()
			// without one, which the seam does not forbid, and it is what
			// makes the render pass' initialLayout true rather than hoped for.
			device_resources.transition_colour(
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			pass.renderPass = device_resources.render_pass();
			pass.framebuffer = device_resources.framebuffer();
			vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

			try
			{
				impl.record_views(commands, 0, impl.view_count, transform_slot,
					vertex_base);
			}
			catch (...)
			{
				vkCmdEndRenderPass(commands);
				throw;
			}

			vkCmdEndRenderPass(commands);

			// The pass' finalLayout says where it left the image, and nothing
			// this class recorded put it there - so the tracking is told
			// rather than asked. It is the same layout it went in as, which is
			// what would make a second pass in one frame legal here - and
			// renderer.h decides that a second submit does nothing on any
			// backend, so what this line actually buys is the read-back path,
			// which reopens the command buffer after this and needs the layout
			// to be true.
			device_resources.set_colour_layout(
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			return;
		}

		// WITH A CHAIN, TWO PASSES. The processed views - every view before
		// begin_overlay's, or every view without one - draw into the scene
		// target; then one pass on the colour target draws the scene through
		// the chain and the overlay views over it, in the same subpass, so
		// the overlay needs no dependency of its own (docs/effects.md#4.5).
		const int post_at = impl.overlay_start >= 0
			? std::min(impl.overlay_start, impl.view_count)
			: impl.view_count;

		device_resources.transition_scene(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		pass.renderPass = device_resources.render_pass();
		pass.framebuffer = device_resources.scene_framebuffer();
		vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

		try
		{
			impl.record_views(commands, 0, post_at, transform_slot,
				vertex_base);
		}
		catch (...)
		{
			vkCmdEndRenderPass(commands);
			throw;
		}

		vkCmdEndRenderPass(commands);
		device_resources.set_scene_layout(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		// THE PRODUCER CARRIES THE DEPENDENCY. The pass' own outgoing
		// dependency is to a transfer read, which is what the colour target
		// needs; the scene is read by the post pass' fragment shader, and
		// this barrier - colour attachment writes to shader reads - is what
		// makes the views' output visible to it.
		device_resources.transition_scene(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		device_resources.transition_colour(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		pass.renderPass = device_resources.output_render_pass();
		pass.framebuffer = device_resources.framebuffer();
		vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

		try
		{
			impl.draw_post(commands, post_frame_offset);
			impl.record_views(commands, post_at, impl.view_count,
				transform_slot, vertex_base);
		}
		catch (...)
		{
			vkCmdEndRenderPass(commands);
			throw;
		}

		vkCmdEndRenderPass(commands);
		device_resources.set_colour_layout(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}

	void Renderer::Impl::record_views(VkCommandBuffer commands, int first,
		int last, size_t& transform_slot, size_t& vertex_base)
	{
		DeviceResources::Frame& frame = this->device_resources.frame();
		const VkDeviceSize transform_stride =
			this->device_resources.owner().uniform_alignment;
		unsigned char* transform_bytes =
			static_cast<unsigned char*>(frame.transforms.mapped);

		// Bound at the first run rather than up front: a range with no run
		// may come before any vertex buffer exists, and the post pass binds a
		// pipeline of its own in the same command buffer.
		bool bound = false;

		// In view order, which is the only ordering guarantee the seam makes -
		// and here it is the only ordering there is, because a view's recording
		// is a vector rather than a command list.
		for (int i = first; i < last; i++)
		{
			const DrawList::View& view = *this->views[static_cast<size_t>(i)];
			const size_t view_vertex_base = vertex_base;
			vertex_base += view.vertices.size();

			for (const DrawList::View::Run& run : view.runs)
			{
				// ONE RECTANGLE, AND EVERYTHING BELOW READS IT. Vulkan takes a
				// float viewport, so this backend could skip pixel_rect - and
				// must not: viewport.h says it is the only conversion to whole
				// pixels any backend may make, and the point of that rule is
				// that all of them make the same one.
				const RectangleI pixels = run.viewport.pixel_rect();

				if (pixels.width <= 0 || pixels.height <= 0)
				{
					// An empty pane draws nothing on every backend. Here it
					// would also be a zero-height viewport, which this API does
					// not define, so it is skipped rather than submitted.
					continue;
				}

				if (!bound)
				{
					vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
						this->pipeline);

					const VkDeviceSize vertex_offset = 0;
					vkCmdBindVertexBuffers(commands, 0, 1,
						&frame.vertices.buffer, &vertex_offset);
					vkCmdBindIndexBuffer(commands, this->indices.buffer, 0,
						VK_INDEX_TYPE_UINT16);
					bound = true;
				}

				// THE NEGATIVE HEIGHT IS THE Y FLIP, AND IT IS THIS BACKEND'S
				// WHOLE SHARE OF THE ONE TERM renderer.h SAYS A BACKEND STILL
				// DECIDES. Vulkan's clip space runs y down where Direct3D's and
				// GL's run up, so the shader those two share - which is this
				// one, unchanged - would put the top of every pane at the
				// bottom. Handing the rasteriser a viewport whose origin is the
				// pane's BOTTOM edge and whose height is negative inverts y
				// once, in the one place a backend is allowed to. The GL
				// backend answers the mirror image of this question by
				// subtracting from the framebuffer height, and says so there.
				VkViewport pane = {};
				pane.x = static_cast<float>(pixels.x);
				pane.y = static_cast<float>(pixels.y + pixels.height);
				pane.width = static_cast<float>(pixels.width);
				pane.height = -static_cast<float>(pixels.height);
				pane.minDepth = run.viewport.minDepth;
				pane.maxDepth = run.viewport.maxDepth;
				vkCmdSetViewport(commands, 0, 1, &pane);

				// The scissor is not optional and is not the viewport: this API
				// clips to both, and a scissor offset may not be negative. The
				// clamp removes negative near edges without moving the pane.
				// Its far edges retain the pane extent; the render area's extent
				// and the colour target bound the visible output. An oversized
				// pane keeps its local pixel scale and is cropped to that output.
				const int scissor_x = std::max(0, pixels.x);
				const int scissor_y = std::max(0, pixels.y);

				VkRect2D scissor = {};
				scissor.offset = { scissor_x, scissor_y };
				scissor.extent.width = static_cast<uint32_t>(std::max(0,
					pixels.x + pixels.width - scissor_x));
				scissor.extent.height = static_cast<uint32_t>(std::max(0,
					pixels.y + pixels.height - scissor_y));
				vkCmdSetScissor(commands, 0, 1, &scissor);

				const VkDeviceSize transform_offset =
					static_cast<VkDeviceSize>(transform_slot) *
					transform_stride;

				float transform[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				fill_transform(transform, pixels);
				std::memcpy(transform_bytes + transform_offset, transform,
					sizeof(transform));
				transform_slot++;

				// A DESCRIPTOR SET PER RUN, THROWN AWAY WITH THE FRAME. D3D12
				// keeps one shader-visible heap and gives every texture a slot
				// in it for the life of the process, which is a fixed capacity
				// and a re-load rule (its render_resources.cpp carries both).
				// This is the other answer: the pool is the frame's, resetting
				// it frees every set at once, and a texture holds no descriptor
				// state at all. What it costs is one allocate and one update
				// per run per frame, on a path that already touches the driver
				// once per run.
				VkDescriptorSet set =
					this->device_resources.allocate_descriptor_set(
						this->set_layout);

				VkDescriptorBufferInfo transform_binding = {};
				transform_binding.buffer = frame.transforms.buffer;
				transform_binding.offset = transform_offset;
				transform_binding.range = sizeof(transform);

				VkDescriptorImageInfo texture_binding = {};
				texture_binding.imageView = run.texture->view();
				texture_binding.imageLayout =
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkDescriptorImageInfo sampler_binding = {};
				sampler_binding.sampler = this->sampler(run.filter);

				VkWriteDescriptorSet writes[3] = {};
				writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[0].dstSet = set;
				writes[0].dstBinding = 0;
				writes[0].descriptorCount = 1;
				writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writes[0].pBufferInfo = &transform_binding;

				writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[1].dstSet = set;
				writes[1].dstBinding = 1;
				writes[1].descriptorCount = 1;
				writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				writes[1].pImageInfo = &texture_binding;

				writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[2].dstSet = set;
				writes[2].dstBinding = 2;
				writes[2].descriptorCount = 1;
				writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				writes[2].pImageInfo = &sampler_binding;

				vkUpdateDescriptorSets(this->device_resources.device(), 3,
					writes, 0, nullptr);

				vkCmdBindDescriptorSets(commands,
					VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline_layout, 0,
					1, &set, 0, nullptr);

				const int32_t base_vertex = static_cast<int32_t>(
					view_vertex_base +
					static_cast<size_t>(run.first_sprite) *
						vertices_per_sprite);

				vkCmdDrawIndexed(commands,
					static_cast<uint32_t>(run.sprites * indices_per_sprite), 1,
					0, base_vertex, 0);
			}
		}
	}

	void Renderer::Impl::draw_post(VkCommandBuffer commands,
		VkDeviceSize frame_offset)
	{
		DeviceResources::Frame& frame = this->device_resources.frame();
		const VkExtent2D extent = this->device_resources.colour_extent();
		const float width = static_cast<float>(extent.width);
		const float height = static_cast<float>(extent.height);

		PostFrameConstants constants = {};
		constants.output_size[0] = width;
		constants.output_size[1] = height;
		constants.output_size[2] = 1.0f / width;
		constants.output_size[3] = 1.0f / height;
		constants.scene_size[0] = width;
		constants.scene_size[1] = height;
		constants.scene_size[2] = 1.0f / width;
		constants.scene_size[3] = 1.0f / height;
		// Every image here is stored top row first: no flip.
		constants.frame_terms[2] = 1.0f;

		// One material a chain until multi-pass chains exist
		// (docs/effects.md#13), so the chain's one effect is the pass.
		const PostEffect effect = this->post_effects.front();
		const PostValues& values =
			this->post_values[static_cast<size_t>(effect)];
		std::memcpy(constants.material_values, values.values.data(),
			sizeof(constants.material_values));
		VkPipeline material_pipeline = this->post_identity_pipeline;

		std::memcpy(static_cast<unsigned char*>(frame.transforms.mapped) +
			frame_offset, &constants, sizeof(constants));

		vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
			material_pipeline);

		// THE SAME NEGATIVE-HEIGHT VIEWPORT THE SPRITES USE, over the whole
		// output: the post vertex entry emits Direct3D-convention clip y
		// (docs/effects.md#5.4), and this is where every pass inverts it.
		VkViewport whole = {};
		whole.x = 0.0f;
		whole.y = height;
		whole.width = width;
		whole.height = -height;
		whole.minDepth = 0.0f;
		whole.maxDepth = 1.0f;
		vkCmdSetViewport(commands, 0, 1, &whole);

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;
		vkCmdSetScissor(commands, 0, 1, &scissor);

		VkDescriptorSet set = this->device_resources.allocate_descriptor_set(
			this->post_set_layout);

		VkDescriptorBufferInfo frame_binding = {};
		frame_binding.buffer = frame.transforms.buffer;
		frame_binding.offset = frame_offset;
		frame_binding.range = sizeof(constants);

		VkDescriptorImageInfo scene_binding = {};
		scene_binding.imageView = this->device_resources.scene_view();
		scene_binding.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo point_binding = {};
		point_binding.sampler = this->point_sampler;

		VkDescriptorImageInfo linear_binding = {};
		linear_binding.sampler = this->linear_sampler;

		VkWriteDescriptorSet writes[4] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = set;
		writes[0].dstBinding = post_frame_binding;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &frame_binding;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = set;
		writes[1].dstBinding = post_input_0_binding;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[1].pImageInfo = &scene_binding;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = set;
		writes[2].dstBinding = post_point_sampler_binding;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writes[2].pImageInfo = &point_binding;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = set;
		writes[3].dstBinding = post_linear_sampler_binding;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writes[3].pImageInfo = &linear_binding;

		vkUpdateDescriptorSets(this->device_resources.device(), 4, writes, 0,
			nullptr);

		vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
			this->post_pipeline_layout, 0, 1, &set, 0, nullptr);

		// One triangle from the vertex index alone; no vertex buffer.
		vkCmdDraw(commands, 3, 1, 0, 0);
	}

	Vector2F Renderer::back_buffer_size() const
	{
		// THE COLOUR TARGET'S, WHICH IS THE SIZE THE SHELL SAID. renderer.h
		// makes this the size of the buffer rather than of the last thing
		// anyone said, and on this backend those two are the same number for a
		// reason no other backend has: the frame is not drawn into a swapchain
		// image, so the presentation engine's opinion of how big the window is
		// does not reach it. device_resources.h carries that whole argument.
		const VkExtent2D extent = this->impl_->device_resources.colour_extent();
		return { static_cast<float>(extent.width),
			static_cast<float>(extent.height) };
	}

	void Renderer::read_back_buffer(std::vector<unsigned char>& pixels)
	{
		Impl& impl = *this->impl_;
		DeviceResources& device_resources = impl.device_resources;

		const VkExtent2D extent = device_resources.colour_extent();
		const size_t width = static_cast<size_t>(extent.width);
		const size_t height = static_cast<size_t>(extent.height);
		pixels.resize(width * height * 4);

		// Made per call and thrown away: this is not a frame-path function and
		// a cached read-back buffer would be one more thing to remake on a
		// device loss.
		// Recovery can replace the coordinating device before returning or
		// throwing. ScopedVulkanBuffer keeps this buffer's creating generation
		// alive and releases it through that device on every way out.
		const ScopedVulkanBuffer readback(device_resources.shared_device(),
			static_cast<VkDeviceSize>(pixels.size()),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			"The read-back buffer");

		// A lost frame must not be read, even though the retained device still
		// permits this buffer's cleanup.
		bool lost = false;

		const VkImageLayout previous = device_resources.colour_layout();

		device_resources.transition_colour(
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		// TIGHTLY PACKED, WHICH IS WHY THERE IS NO ROW PITCH HERE AND THERE
		// IS ONE ON D3D12. A zero bufferRowLength means "as wide as the
		// image", so the copy lands exactly width * height * 4 bytes with
		// nothing between the rows - where a D3D12 copy pads every row to
		// 256 bytes and the runtime is the only thing that knows to what.
		VkBufferImageCopy copy = {};
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.layerCount = 1;
		copy.imageExtent = { extent.width, extent.height, 1 };

		vkCmdCopyImageToBuffer(device_resources.commands(),
			device_resources.colour_target(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer().buffer,
			1, &copy);

		// AND THE HOST IS A DOMAIN THE WAIT BELOW DOES NOT REACH. A
		// timeline wait is an EXECUTION dependency: it says the copy has
		// finished, not that what it wrote is available to a CPU load.
		// HOST_COHERENT makes the write visible without making it
		// available, and there is no other barrier into the host domain
		// anywhere in this folder. This is the one hazard on this path that
		// synchronization validation cannot report, because it cannot
		// observe a CPU read - and it is the path every golden image goes
		// through.
		VkBufferMemoryBarrier to_host = {};
		to_host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		to_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		to_host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_host.buffer = readback.buffer().buffer;
		to_host.offset = 0;
		to_host.size = VK_WHOLE_SIZE;

		vkCmdPipelineBarrier(device_resources.commands(),
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
			0, nullptr, 1, &to_host, 0, nullptr);

		// PUT BACK WHERE IT WAS, not into a layout this function decided.
		// The seam says this is called between submit() and end_frame(), so
		// the frame is not over; a caller that reads twice, or that reads
		// and then presents, gets what it would have got without the read.
		// UNDEFINED is not a layout anything can be transitioned INTO, so a
		// colour target nothing has drawn into yet is simply left where the
		// copy put it.
		if (previous != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			device_resources.transition_colour(previous);
		}

		// False means recovery rebuilt the renderer. Its old frame cannot
		// supply pixels, and a recovery exception must keep its own error.
		lost = !device_resources.execute();

		if (!lost)
		{
			// It stalls on the GPU by construction, and the seam says so.
			device_resources.wait_for_gpu();

			// B AND R SWAP ON THE WAY OUT: the colour target is B8G8R8A8
			// and the seam promises RGBA. No flip, though - Vulkan's
			// framebuffer origin is the top left like Direct3D's, so the
			// row order is already the one the seam asks for and only the
			// GL backend has to turn its rows over.
			const unsigned char* bytes =
				static_cast<const unsigned char*>(readback.buffer().mapped);
			for (size_t i = 0; i < width * height; i++)
			{
				pixels[i * 4 + 0] = bytes[i * 4 + 2];
				pixels[i * 4 + 1] = bytes[i * 4 + 1];
				pixels[i * 4 + 2] = bytes[i * 4 + 0];
				pixels[i * 4 + 3] = bytes[i * 4 + 3];
			}
		}

		if (lost)
		{
			// Report the loss instead of returning a black frame (T6).
			throw std::runtime_error("The device was lost while reading the "
				"back buffer, so there is nothing to read: everything the "
				"renderer holds has been rebuilt and the frame that was being "
				"read no longer exists.");
		}
	}

	// THE THREE MARKERS DO NOTHING, AND THAT IS THE HONEST ANSWER RATHER THAN A
	// GAP - which is worth arguing here rather than pointing at the D3D12 and
	// GL backends that say it, because this is the one API that HAS the feature.
	// VK_EXT_debug_utils' labels are real and are in the box. What they are not
	// is available where this seam puts them WITHOUT A CHOICE THIS FILE WOULD
	// HAVE TO MAKE: the useful label belongs to a COMMAND BUFFER, and these
	// three may be called at any point a client likes, including when no frame
	// is open and nothing is recording. The extension does have a queue-level
	// pair - vkQueueBeginDebugUtilsLabelEXT and its end - so a marker outside a
	// frame COULD be forwarded there; what it could not be is the same thing as
	// a marker inside one, which would make the meaning of a marker depend on
	// where it was called. D3D11 has no such split, because
	// ID3DUserDefinedAnnotation hangs off the device context and is always
	// there. So the honest choices here are a marker that means two things or
	// one that never does anything, and renderer.h settles the axis
	// for all five: markers are advisory, a backend may discard them, and this
	// one does (T6, T9 - the extension is optional and nothing in this
	// repository reads a capture).
	void Renderer::begin_marker(const wchar_t* name)
	{
		std::ignore = name;
	}

	void Renderer::end_marker()
	{

	}

	void Renderer::set_marker(const wchar_t* name)
	{
		std::ignore = name;
	}

	// --- the post stage (docs/effects.md) ------------------------------------

	void Renderer::set_post_effects(std::span<const PostEffect> effects)
	{
		check_post_chain(effects, this->impl_->frame_begun);

		this->impl_->post_effects.assign(effects.begin(), effects.end());
		this->impl_->device_resources.set_scene_target(
			!this->impl_->post_effects.empty());
	}

	void Renderer::set_post_values(PostEffect effect, const PostValues& values)
	{
		check_post_values(this->impl_->post_effects, effect, values);
		this->impl_->post_values[static_cast<size_t>(effect)] = values;
	}

	void Renderer::begin_overlay()
	{
		Impl& impl = *this->impl_;
		check_begin_overlay(impl.frame_begun, impl.overlay_start,
			impl.view_count, static_cast<int>(impl.views.size()));
		const int overlay = impl.view_count;

		// EVERY VIEW FROM THE OVERLAY UP STARTS AFRESH, as on the other
		// backends, where one bound earlier this frame still holds the scene
		// target. Here a view records memory and submit() decides its pass by
		// index, so what this drops is any camera, filter or viewport a view
		// declared earlier in the frame and then dropped by a lowered count.
		const VkExtent2D extent = impl.device_resources.colour_extent();
		const int capacity = static_cast<int>(impl.views.size());
		for (int i = overlay; i < capacity; i++)
		{
			DrawList::View& view = *impl.views[static_cast<size_t>(i)];
			view.reset();
			view.viewport = Viewport(0.0f, 0.0f,
				static_cast<float>(extent.width),
				static_cast<float>(extent.height));
		}

		impl.overlay_start = overlay;
		this->set_view_count(overlay + 1);
	}
}

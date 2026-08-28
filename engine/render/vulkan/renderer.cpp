#include "engine/render/vulkan/backend.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/sprite_geometry.h"
#include "engine/render/sprite_vertex.h"

// The shaders, as bytes this build compiled (cmake/compile_shaders.cmake) from
// engine/render/sprite.hlsl - the same source the two Direct3D backends
// compile, through a different compiler into a different intermediate. That
// file says why it is one file and not three, and compile_shaders.cmake says
// what reaching SPIR-V from HLSL cost and which of the two dxc.exe on a Windows
// developer's machine can do it.
#include "engine/render/vulkan/sprite_pixel_shader.h"
#include "engine/render/vulkan/sprite_vertex_shader.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		// Four corners, two triangles, and the winding the corner order in
		// sprite_geometry.h fixes.
		const int VERTICES_PER_SPRITE = 4;
		const int INDICES_PER_SPRITE = 6;

		// Whether two viewports are the same pane. A run cannot span a viewport
		// change, so this is what closes one.
		bool same_viewport(const Viewport& left, const Viewport& right)
		{
			return left.x == right.x && left.y == right.y &&
				left.width == right.width && left.height == right.height &&
				left.minDepth == right.minDepth &&
				left.maxDepth == right.maxDepth;
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
			const float width = static_cast<float>(pixels.width);
			const float height = static_cast<float>(pixels.height);

			transform[0] = width > 0.0f ? 2.0f / width : 0.0f;
			transform[1] = height > 0.0f ? -2.0f / height : 0.0f;
			transform[2] = -1.0f;
			transform[3] = 1.0f;
		}

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

	// A DRAW TOUCHES NO DRIVER. Everything a view records is memory: the four
	// corners the engine's geometry produced, and a run saying what to draw them
	// with. That is what lets several workers record at once into an API whose
	// command pools are single-threaded by specification, and it is the OpenGL
	// backend's shape rather than D3D11's - engine/render/vulkan/backend.h says
	// why that is a decision and not a shortcut.
	void DrawList::View::draw(const VulkanTexture& texture,
		const SpriteVertex* corners)
	{
		const int first =
			static_cast<int>(this->vertices.size()) / VERTICES_PER_SPRITE;

		const bool joins = this->open_valid &&
			this->open.texture == &texture &&
			this->open.filter == this->filter &&
			same_viewport(this->open.viewport, this->viewport) &&
			this->open.sprites < MAX_RUN_SPRITES;

		if (!joins)
		{
			this->close_run();
			this->open.viewport = this->viewport;
			this->open.texture = &texture;
			this->open.filter = this->filter;
			this->open.first_sprite = first;
			this->open.sprites = 0;
			this->open_valid = true;
		}

		this->vertices.insert(this->vertices.end(), corners,
			corners + VERTICES_PER_SPRITE);
		this->open.sprites++;
		this->touched = true;
	}

	void DrawList::View::close_run()
	{
		if (!this->open_valid || this->open.sprites == 0)
		{
			this->open_valid = false;
			return;
		}
		this->runs.push_back(this->open);
		this->open_valid = false;
	}

	void DrawList::View::reset()
	{
		this->camera = Camera::DEFAULT_CAMERA;
		this->filter = TextureFilter::point;
		this->vertices.clear();
		this->runs.clear();
		this->open_valid = false;
		this->touched = false;
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
		// UNUSED, AND SAYING SO IS THE POINT. layer_depth orders nothing -
		// RenderPixelTests pins that call order decides - and the sprite vertex
		// has no z for it to ride in. It stays on the seam because the seam is
		// what a client writes against.
		std::ignore = layer_depth;

		const VulkanTexture& resolved =
			*this->view_->owner->resources->impl()->texture(texture);

		SpriteVertex corners[4];
		build_sprite_quad(
			this->view_->camera.calculate_view_rectangle(destination),
			source, resolved.size(), tint, rotation, origin, flip, corners);

		this->view_->draw(resolved, corners);
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

		// The font table is the seam's own and the texture table is this
		// folder's, which is the rule about which half lives where reading as
		// two lines of code (engine/render/render_resources.h).
		const RenderResources& resources = *this->view_->owner->resources;
		const Font& the_font = *resources.font(font);
		const VulkanTexture& atlas =
			*resources.impl()->texture(the_font.atlas());
		const Vector2F atlas_size = atlas.size();

		const Vector2F screen_position =
			this->view_->camera.calculate_view_position(position);
		const float screen_scale =
			this->view_->camera.calculate_view_scale(scale);

		// IDENTICAL TO EVERY OTHER BACKEND'S, LINE FOR LINE BAR THE TYPES,
		// which is the point of the walk and the quad both being the engine's.
		// A glyph is a sprite; what is left here is resolving two handles.
		DrawList::View& view = *this->view_;
		the_font.for_each_glyph(text,
			[&](const Glyph& glyph, const Vector2F& pen)
			{
				SpriteVertex corners[4];
				build_glyph_quad(screen_position, screen_scale, glyph, pen,
					atlas_size, tint, rotation, origin, corners);

				view.draw(atlas, corners);
			});
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
		// destroy_device_dependent_resources, which this paragraph used to
		// claim and which that function has never contained. It matters
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

		check_vk(vkCreateDescriptorSetLayout(device, &set_description, nullptr,
			&this->set_layout),
			"The sprite descriptor set layout could not be created");

		VkPipelineLayoutCreateInfo layout_description = {};
		layout_description.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_description.setLayoutCount = 1;
		layout_description.pSetLayouts = &this->set_layout;

		check_vk(vkCreatePipelineLayout(device, &layout_description, nullptr,
			&this->pipeline_layout),
			"The sprite pipeline layout could not be created");

		// --- The pipeline ----------------------------------------------------
		//
		// ONE OBJECT, AS ON D3D12, AND MADE ONCE. The blend, the rasteriser
		// state, the vertex layout and both shaders are fields of it, so there
		// is nothing to bind per draw call that the D3D11 backend binds. The
		// two samplers are the exception and are not in here: the seam lets a
		// draw list change its filter mid-list, so they are descriptors a draw
		// call names rather than state a pipeline fixes.
		VkShaderModule vertex_shader = create_shader(device,
			SPRITE_VERTEX_SHADER, sizeof(SPRITE_VERTEX_SHADER),
			"The sprite vertex shader could not be created");
		VkShaderModule pixel_shader = create_shader(device,
			SPRITE_PIXEL_SHADER, sizeof(SPRITE_PIXEL_SHADER),
			"The sprite pixel shader could not be created");

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

		// NO CULLING, WHERE SpriteBatch CULLED BACK FACES. A sprite is a quad
		// whose winding never changes: a flip mirrors the texture coordinates
		// and leaves the corners where they were (sprite_geometry.cpp). It
		// matters more here than anywhere else, because the negative viewport
		// height in submit() reverses the winding as it reverses y - so a
		// backend that culled would have to pick the opposite face from every
		// other one and would look correct until somebody changed the flip.
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
		// because both answers look plausible and every opaque sprite in both
		// samples renders identically either way. It is the same two words in
		// five files now.
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

		const VkResult built = vkCreateGraphicsPipelines(device,
			VK_NULL_HANDLE, 1, &description, nullptr, &this->pipeline);

		vkDestroyShaderModule(device, vertex_shader, nullptr);
		vkDestroyShaderModule(device, pixel_shader, nullptr);

		check_vk(built, "The sprite pipeline could not be created");

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
		check_vk(vkCreateSampler(device, &sampler_description, nullptr,
			&this->point_sampler),
			"The point sampler could not be created");

		sampler_description.magFilter = VK_FILTER_LINEAR;
		sampler_description.minFilter = VK_FILTER_LINEAR;
		check_vk(vkCreateSampler(device, &sampler_description, nullptr,
			&this->linear_sampler),
			"The linear sampler could not be created");

		// --- The index buffer -------------------------------------------------
		//
		// The same two triangles per sprite for every sprite there will ever
		// be, so it is filled once and never touched again. IN DEVICE-LOCAL
		// MEMORY AND COPIED THERE, where D3D11 says IMMUTABLE and hands over
		// the bytes: this API has no initial-data parameter either, so
		// "upload it once at creation" is a staging buffer, a copy on a command
		// buffer and a wait - the same three steps every texture takes
		// (texture_factory.cpp).
		std::vector<unsigned short> index_data;
		index_data.reserve(static_cast<size_t>(DrawList::View::MAX_RUN_SPRITES)
			* INDICES_PER_SPRITE);
		for (int sprite = 0; sprite < DrawList::View::MAX_RUN_SPRITES;
			sprite++)
		{
			const unsigned short first =
				static_cast<unsigned short>(sprite * VERTICES_PER_SPRITE);
			index_data.push_back(static_cast<unsigned short>(first + 0));
			index_data.push_back(static_cast<unsigned short>(first + 1));
			index_data.push_back(static_cast<unsigned short>(first + 2));
			index_data.push_back(static_cast<unsigned short>(first + 1));
			index_data.push_back(static_cast<unsigned short>(first + 3));
			index_data.push_back(static_cast<unsigned short>(first + 2));
		}

		const VkDeviceSize index_bytes =
			static_cast<VkDeviceSize>(index_data.size()) *
			sizeof(unsigned short);

		this->indices = create_vulkan_buffer(this->device_resources.owner(),
			index_bytes,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "The sprite index buffer");

		VulkanBuffer staging = create_vulkan_buffer(
			this->device_resources.owner(),
			index_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			"The sprite index buffer's staging copy");

		// AND EVERY FAILURE FROM HERE TO THE RELEASE HAS TO PUT IT BACK, which
		// is texture_factory.cpp's sentence over the identical three steps.
		// begin_upload, end_upload and the wait inside it all throw, and a
		// VulkanBuffer has no destructor to lean on. This path also runs inside
		// handle_device_lost's restore, where a leak is a leak per loss rather
		// than one per process.
		try
		{
			std::memcpy(staging.mapped, index_data.data(),
				static_cast<size_t>(index_bytes));

			VkCommandBuffer upload = this->device_resources.begin_upload();

			VkBufferCopy copy = {};
			copy.size = index_bytes;
			vkCmdCopyBuffer(upload, staging.buffer, this->indices.buffer, 1,
				&copy);

			// TRANSFER_WRITE -> INDEX_READ, WHICH IS THE SAME BARRIER THE
			// TEXTURE PATH EMITS AND FOR THE SAME REASON. end_upload waits for
			// the whole queue afterwards, so this looks redundant - and it is
			// exactly as redundant as texture_factory.cpp's TRANSFER ->
			// FRAGMENT_SHADER barrier, which is there because a host wait is
			// not an availability operation for a later queue access. The
			// asymmetry was the finding; a buffer this engine reads on every
			// draw call it ever makes is not the place to be the exception.
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
		}
		catch (...)
		{
			destroy_vulkan_buffer(device, staging);
			throw;
		}

		destroy_vulkan_buffer(device, staging);
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
		this->device_resources.transition_colour(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

		this->device_resources.transition_colour(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		// The views the frame will draw into, against the buffer that now
		// exists. view_count is deliberately not touched: begin_frame sets it
		// to zero before calling this and a resize must not, because the layout
		// is the shell's to decide and the resize is what tells it to decide
		// again (renderer.h, window_size_changed).
		this->reset_views();
	}

	void Renderer::Impl::on_device_lost()
	{
		this->destroy_device_dependent_resources();

		if (this->notify != nullptr)
		{
			this->notify->on_device_lost();
		}
	}

	void Renderer::Impl::on_device_restored()
	{
		this->create_device_dependent_resources();

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
		if (view_capacity < 1)
		{
			throw std::invalid_argument(
				"Renderer::create_device needs a view capacity of at least 1.");
		}

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
	}

	void Renderer::set_view_count(int count)
	{
		const int capacity = static_cast<int>(this->impl_->views.size());
		if (count < 0 || count > capacity)
		{
			throw std::out_of_range("Renderer::set_view_count(" +
				std::to_string(count) + ") is outside the capacity of " +
				std::to_string(capacity) + " create_device was given.");
		}

		// Lowering it past a view something has already drawn into strands that
		// recording: submit() would not replay it. The two backends that record
		// into a command list have a harder version of this problem, but the
		// answer the seam gives is the same, so it is the same throw.
		for (int i = count; i < this->impl_->view_count; i++)
		{
			if (this->impl_->views[static_cast<size_t>(i)]->touched)
			{
				throw std::logic_error("Renderer::set_view_count(" +
					std::to_string(count) + ") would drop view " +
					std::to_string(i) + ", which has already been drawn into "
					"this frame.");
			}
		}

		this->impl_->view_count = count;
	}

	int Renderer::view_count() const
	{
		return this->impl_->view_count;
	}

	DrawList Renderer::view(int index) const
	{
		if (index < 0 || index >= this->impl_->view_count)
		{
			throw std::out_of_range("Renderer::view(" + std::to_string(index) +
				") - this frame has " +
				std::to_string(this->impl_->view_count) + " views. Asking for "
				"one nobody set up used to answer with a fullscreen pane and "
				"draw a whole extra pass.");
		}
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

		if (total_runs == 0)
		{
			return;
		}

		const VkDeviceSize transform_stride =
			device_resources.owner().uniform_alignment;

		device_resources.reserve_vertices(
			static_cast<VkDeviceSize>(total_vertices) * sizeof(SpriteVertex));
		device_resources.reserve_transforms(
			static_cast<VkDeviceSize>(total_runs) * transform_stride);

		DeviceResources::Frame& frame = device_resources.frame();

		// A run in this frame's vertex buffer is addressed by a vertex offset
		// on the draw call, which is what makes one buffer and one binding
		// serve every view - the same trick glDrawElementsBaseVertex plays next
		// door, spelt as a parameter rather than as a separate entry point.
		unsigned char* vertex_bytes =
			static_cast<unsigned char*>(frame.vertices.mapped);
		unsigned char* transform_bytes =
			static_cast<unsigned char*>(frame.transforms.mapped);

		size_t vertex_base = 0;
		std::vector<size_t> view_vertex_base(
			static_cast<size_t>(impl.view_count), 0);

		for (int i = 0; i < impl.view_count; i++)
		{
			const DrawList::View& view = *impl.views[static_cast<size_t>(i)];
			view_vertex_base[static_cast<size_t>(i)] = vertex_base;

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

		// A no-op in every ordinary frame - open_frame left it a colour
		// attachment. It is here for the frame that reaches submit() without
		// one, which the seam does not forbid, and it is what makes the render
		// pass' initialLayout true rather than hoped for.
		device_resources.transition_colour(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		const VkExtent2D extent = device_resources.colour_extent();

		VkRenderPassBeginInfo pass = {};
		pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		pass.renderPass = device_resources.render_pass();
		pass.framebuffer = device_resources.framebuffer();
		pass.renderArea.offset = { 0, 0 };
		pass.renderArea.extent = extent;

		vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
			impl.pipeline);

		const VkDeviceSize vertex_offset = 0;
		vkCmdBindVertexBuffers(commands, 0, 1, &frame.vertices.buffer,
			&vertex_offset);
		vkCmdBindIndexBuffer(commands, impl.indices.buffer, 0,
			VK_INDEX_TYPE_UINT16);

		size_t transform_slot = 0;

		// In view order, which is the only ordering guarantee the seam makes -
		// and here it is the only ordering there is, because a view's recording
		// is a vector rather than a command list.
		for (int i = 0; i < impl.view_count; i++)
		{
			const DrawList::View& view = *impl.views[static_cast<size_t>(i)];

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
				// clamp is the intersection with the buffer rather than a move,
				// so a pane that starts off the left edge loses the part that
				// was never in the picture and keeps the rest.
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
				VkDescriptorSet set = device_resources.allocate_descriptor_set(
					impl.set_layout);

				VkDescriptorBufferInfo transform_binding = {};
				transform_binding.buffer = frame.transforms.buffer;
				transform_binding.offset = transform_offset;
				transform_binding.range = sizeof(transform);

				VkDescriptorImageInfo texture_binding = {};
				texture_binding.imageView = run.texture->view();
				texture_binding.imageLayout =
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkDescriptorImageInfo sampler_binding = {};
				sampler_binding.sampler = impl.sampler(run.filter);

				VkWriteDescriptorSet writes[3] = {};
				writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[0].dstSet = set;
				writes[0].dstBinding = 0;
				writes[0].descriptorCount = 1;
				writes[0].descriptorType =
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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

				vkUpdateDescriptorSets(device_resources.device(), 3, writes, 0,
					nullptr);

				vkCmdBindDescriptorSets(commands,
					VK_PIPELINE_BIND_POINT_GRAPHICS, impl.pipeline_layout, 0,
					1, &set, 0, nullptr);

				const int32_t base_vertex = static_cast<int32_t>(
					view_vertex_base[static_cast<size_t>(i)] +
					static_cast<size_t>(run.first_sprite) *
						VERTICES_PER_SPRITE);

				vkCmdDrawIndexed(commands,
					static_cast<uint32_t>(run.sprites * INDICES_PER_SPRITE), 1,
					0, base_vertex, 0);
			}
		}

		vkCmdEndRenderPass(commands);

		// The pass' finalLayout says where it left the image, and nothing this
		// class recorded put it there - so the tracking is told rather than
		// asked. It is the same layout it went in as, which is what would make
		// a second pass in one frame legal here - and renderer.h has since
		// decided that a second submit does nothing on any backend, so what
		// this line actually buys is the read-back path, which reopens the
		// command buffer after this and needs the layout to be true.
		device_resources.set_colour_layout(
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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
		VulkanBuffer readback = create_vulkan_buffer(device_resources.owner(),
			static_cast<VkDeviceSize>(pixels.size()),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			"The read-back buffer");

		// WHETHER THE DEVICE WENT AWAY UNDER THIS READ, decided inside the try
		// below and answered after it. It cannot be answered inside, because
		// the answer is precisely that nothing may touch `readback` again.
		bool lost = false;

		// A VulkanBuffer HAS NO DESTRUCTOR AND THERE ARE SIX THROWING CALLS
		// BELOW IT, which is the whole of why this block is here. The same
		// sentence texture_factory.cpp writes over its staging copy: there is
		// no ComPtr in this API, so every failure between a handle being taken
		// and being released has to put it back by hand.
		try
		{
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
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1,
				&copy);

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
			to_host.buffer = readback.buffer;
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

			// THE ANSWER IS NOT A COURTESY, AND device_resources.h SAYS SO ON
			// execute() ITSELF: false means the device was lost and everything
			// has been rebuilt on the way out - the VkDevice these two handles
			// came from included. Discarding it left `readback.mapped` pointing
			// into freed memory and handed a destroyed device's VkBuffer and
			// VkDeviceMemory to its replacement to free.
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
					static_cast<const unsigned char*>(readback.mapped);
				for (size_t i = 0; i < width * height; i++)
				{
					pixels[i * 4 + 0] = bytes[i * 4 + 2];
					pixels[i * 4 + 1] = bytes[i * 4 + 1];
					pixels[i * 4 + 2] = bytes[i * 4 + 0];
					pixels[i * 4 + 3] = bytes[i * 4 + 3];
				}
			}
		}
		catch (...)
		{
			// The device is still the one that made it, so this is the ordinary
			// release. The lost case below is the one that is not.
			destroy_vulkan_buffer(device_resources.device(), readback);
			throw;
		}

		if (lost)
		{
			// DELIBERATELY NOT DESTROYED. Both handles belong to a VkDevice
			// that no longer exists, and the only device to free them against
			// is a different one - which is a call into a driver with another
			// object's handles, the one shape of undefined behaviour this API
			// gives no diagnostic for. Their memory went with the device.
			//
			// AND IT SAYS SO RATHER THAN HANDING BACK A BLACK FRAME (T6). A
			// read that silently answers zeroes fails a golden comparison with
			// a message about pixels instead of about the device.
			throw std::runtime_error("The device was lost while reading the "
				"back buffer, so there is nothing to read: everything the "
				"renderer holds has been rebuilt and the frame that was being "
				"read no longer exists.");
		}

		destroy_vulkan_buffer(device_resources.device(), readback);
	}

	// THE THREE MARKERS DO NOTHING, AND THAT IS THE HONEST ANSWER RATHER THAN A
	// GAP - which is worth arguing here rather than pointing at the other three
	// backends that say it, because this is the one API that HAS the feature.
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
	// one that never does anything, and renderer.h has since settled the axis
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
}

#pragma once

#include "engine/core/registry.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/font.h"
#include "engine/render/renderer.h"
#include "engine/render/render_resources.h"
#include "engine/render/sprite_vertex.h"
#include "engine/render/viewport.h"
#include "engine/render/vulkan/device_resources.h"

#include <memory>
#include <string>
#include <vector>

// The Vulkan backend, for the files that have to name a device.
//
// renderer.h promises that reaching for the device is "a deliberate include of
// engine/render/<backend>/ and not something a game file can do by accident".
// This is that include, and EVERY CLIENT OF IT IS IN THIS FOLDER - which is the
// rule rather than a count. cmake/check_engine_includes.cmake fails the build
// for any file outside engine/render/<backend>/ that names any header inside
// it: the folder, not one filename in it, which is why a fifth backend needed
// no edit to that script.
//
// WHY A FIFTH BACKEND EXISTED TO BE WRITTEN, given that CLAUDE.md said one
// "would need a reason of its own". docs/port/android.md is the reason and it
// is not "Vulkan is modern": it is the one API in that list which reaches
// Android, Linux and - through MoltenVK - the Apple platforms, so an engine
// that has it has stopped needing a backend per platform. That document also
// names the two things this port was expected to cost, and both are now
// measured rather than estimated: SPIR-V at build time is a toolchain the tree
// did not have
// (cmake/compile_shaders.cmake says what that took), and there is no in-box
// software rasteriser the way Direct3D has WARP, so this preset skips
// RenderPixelTests in CI exactly as the OpenGL one does.
//
// WHAT THIS BACKEND ANSWERS THAT NONE OF THE OTHER FOUR COULD. Every one of
// them learns about a resize from Win32 and from nowhere else; renderer.h's
// window_size_changed is written entirely in terms of a caller who already
// knows the size changed. Vulkan is the first API behind this seam where the
// PRESENTATION ENGINE says so instead - VK_ERROR_OUT_OF_DATE_KHR comes back
// from an acquire or a present, on a frame no window message touched. That was
// the open question docs/port/android.md left for the fifth backend, and the
// answer is in device_resources.h: the seam's contract has the right shape,
// because what goes stale is a swapchain and what the seam calls the back
// buffer is not one here. Not a line of renderer.h moved.
//
// THE RECORDING SHAPE IS OPENGL'S AND THE NULL BACKEND'S, NOT D3D11'S. A view
// records into plain memory - vertices, and a list of what to draw them with -
// and submit() replays the views in order on the one thread that owns the
// queue. That is not a shortcut: a VkCommandPool may not be used from two
// threads at once, so per-view recording would mean a pool per view per frame
// in flight, and the seam's parallelism axis is views precisely because a
// view's vertices are already built on the CPU before any backend sees them
// (engine/render/sprite_geometry.h). Memory is thread-safe by construction
// rather than by a rule about pools.
//
// The rest, each written down where it happens: the frame is drawn into an
// image this engine owns and blitted into a swapchain image at present
// (device_resources.h, and it is the largest decision in the port); the
// transform reaches b0 as a uniform buffer rather than as push constants,
// because push constants would need an annotation in the shared
// engine/render/sprite.hlsl (renderer.cpp, create_device_dependent_resources);
// a descriptor set is allocated per run from a pool the frame resets, so a
// texture carries no descriptor state at all (renderer.cpp, submit); the
// viewport is flipped rather than the clip space, which is the one term every
// backend decides for itself (renderer.cpp, submit); block compression is a
// device feature that is asked for and named in the throw if it is absent
// (texture_factory.cpp); and the seam's three debug markers do nothing.

namespace labrador
{
	// A texture, owned: the image, its memory, its view, and how big it is.
	//
	// AND A REFERENCE TO THE DEVICE, WHICH IS THE ONE FIELD NO OTHER BACKEND'S
	// TEXTURE HAS. render_resources.h makes it a term of the seam that a
	// RenderResources outlives the Renderer it was filled against, and closes
	// by saying a texture needs nothing of the renderer because "an
	// ID3D12Resource holds its own reference on the device". A VkImage holds
	// none: it is a handle, and vkDestroyImage takes the device as an argument.
	// So the reference COM gives that backend for free is a shared_ptr here,
	// and device_resources.h carries the argument.
	//
	// The size rides along because engine/render/sprite_geometry.h needs it
	// every draw and this API will not answer for an image it has already made.
	class VulkanTexture
	{
	public:
		VulkanTexture(std::shared_ptr<VulkanDevice> owner, VkImage image,
			VkDeviceMemory memory, VkImageView view, int width, int height)
			: owner_(std::move(owner)), image_(image), memory_(memory),
			  view_(view), width_(width), height_(height) {}
		~VulkanTexture();

		VulkanTexture(const VulkanTexture&) = delete;
		VulkanTexture& operator=(const VulkanTexture&) = delete;

		VkImageView view() const { return this->view_; }
		mattmath::Vector2F size() const;

	private:
		std::shared_ptr<VulkanDevice> owner_;
		VkImage image_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		VkImageView view_ = VK_NULL_HANDLE;
		int width_ = 0;
		int height_ = 0;
	};

	// Where a named texture lives, and only a texture. The font and sheet
	// tables are RenderResources' own members; engine/render/render_resources.h
	// says why, and what it saved.
	class RenderResources::Impl
	{
	public:
		void add_texture(const std::string& name,
			std::unique_ptr<VulkanTexture> texture);

		void release_all_textures();

		const VulkanTexture* texture(TextureHandle texture) const;

		Registry<VulkanTexture> textures{ "Texture" };
	};

	// One view's recording, which is memory and nothing else.
	class DrawList::View
	{
	public:
		// A run of sprites that share everything a draw call fixes. Closed and
		// pushed when any of the three changes, or at the end of the view.
		struct Run
		{
			Viewport viewport;
			const VulkanTexture* texture = nullptr;
			TextureFilter filter = TextureFilter::point;
			int first_sprite = 0;
			int sprites = 0;
		};

		// How many sprites one run may hold. The index buffer is built once for
		// this many and reused with a vertex offset, so a longer run is split
		// rather than growing anything. The same 2048 the other three drawing
		// backends use, arrived at the same way: 16-bit indices cap a run at
		// 16384 sprites and 2048 of them is 256KB of vertices.
		static const int MAX_RUN_SPRITES = 2048;

		Renderer::Impl* owner = nullptr;

		std::vector<SpriteVertex> vertices;
		std::vector<Run> runs;

		// The run being built. Not in `runs` until something closes it.
		Run open;
		bool open_valid = false;

		// Reset at the top of every frame, not carried across one.
		Camera camera = Camera::DEFAULT_CAMERA;
		TextureFilter filter = TextureFilter::point;
		Viewport viewport;

		// Whether anything has been recorded into this view this frame. It is
		// what makes a dropped view detectable - see Renderer::set_view_count.
		bool touched = false;

		void draw(const VulkanTexture& texture, const SpriteVertex* corners);
		void close_run();
		void reset();
	};

	// The device, the swapchain, the views, and everything a draw call needs
	// that is the same for every draw call.
	//
	// It implements VulkanDeviceNotify itself and forwards to the seam's
	// DeviceNotify, so the game hears about a device loss without ever hearing
	// what a device is.
	class Renderer::Impl final : public VulkanDeviceNotify
	{
	public:
		Impl();
		~Impl();

		DeviceResources device_resources;

		std::vector<std::unique_ptr<DrawList::View>> views;
		int view_count = 0;

		// BETWEEN begin_frame AND end_frame, WHICH IS THE INTERVAL renderer.h
		// LEGISLATES AND NOT ONE THE COMMAND BUFFER CAN ANSWER FOR. A resize
		// arriving before the first set_view_count still has a frame to
		// restart, and DeviceResources::recording() is not the question:
		// engine/render/d3d12/backend.h can say its command lists answer no,
		// because its open_frame executes the frame list on the way out, but
		// here open_frame leaves the buffer open and recording() is true from
		// the frame's first barrier onwards. It also goes FALSE mid-frame,
		// every time read_back_buffer submits, which is the half that would
		// actually get this wrong: a resize arriving in that window would find
		// no frame to restart when there is one. So the frame is tracked rather
		// than inferred, and this is the whole of the tracking - the same term
		// as D3D12's, reached for a different reason.
		bool frame_begun = false;

		// Whether submit() has already run for this frame. renderer.h makes a
		// second submit a no-op; this is the whole of what that costs.
		bool frame_submitted = false;

		// SHARED BY EVERY VIEW, because nothing writes to any of them once they
		// are made. The index buffer is the same two triangles per sprite for
		// every sprite there will ever be, uploaded once at device creation.
		VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkSampler point_sampler = VK_NULL_HANDLE;
		VkSampler linear_sampler = VK_NULL_HANDLE;
		VulkanBuffer indices;

		const RenderResources* resources = nullptr;
		labrador::DeviceNotify* notify = nullptr;

		VkSampler sampler(TextureFilter filter) const;

		// Everything above that belongs to the device, remade with it.
		void create_device_dependent_resources();
		void destroy_device_dependent_resources();

		// Clears the colour target and reopens every view against it. The
		// second half of begin_frame, and the whole of what putting a resized
		// frame back on its feet takes.
		void open_frame();

		// Drops what every view recorded, which on this backend is clearing a
		// vector per view and nothing more.
		void reset_views();

		// The same, plus the frame's command buffer - which is only safe once
		// something has waited for the GPU, so this has exactly one caller.
		// begin_frame, past wait_for_frame. The resize path wants the same two
		// things and cannot use this, because the wait it needs is stronger and
		// happens inside DeviceResources; Renderer::window_size_changed says so
		// where it takes them apart.
		void abandon_recording();

		void on_device_lost() override;
		void on_device_restored() override;
	};
}

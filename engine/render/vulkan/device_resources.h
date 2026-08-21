#pragma once

// Before <vulkan/vulkan.h>, because that header pulls in vulkan_win32.h only
// when this is defined and the Win32 surface entry points come with it. It is
// here rather than in engine/CMakeLists.txt so that this header is self-
// contained: a file that includes it gets a working VkSurfaceKHR whatever the
// build says.
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <Windows.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The Vulkan instance, the device, the surface, the swapchain and the timeline.
//
// THE PART OF AN API THAT IS NOT ABOUT DRAWING, which is what renderer.h says a
// backend's fourth translation unit is for. Both Direct3D folders have one of
// these and the GL folder has gl_functions.cpp; this is the same kind of file,
// holding everything that exists before a frame does and outlives every frame
// after it. It is written the way CONVENTIONS says - snake_case, trailing
// underscore on privates, accessors named for the noun - for the reason
// engine/render/d3d12/device_resources.h gives: nothing here is vendored, so
// nothing here has somebody else's style to preserve.
//
// WHAT THIS BACKEND OWNS THAT NO OTHER ONE DOES, and it is one thing rather
// than a list: THE IMAGE THE FRAME IS DRAWN INTO IS NOT A SWAPCHAIN IMAGE. On
// the other three, "the back buffer" and "the thing the presentation engine
// hands out" are the same object - a DXGI buffer, or a WGL context's default
// framebuffer. Here they are two, and every awkward property of this API's
// presentation model follows from insisting they are one:
//
//  - A Vulkan swapchain image must be ACQUIRED before it is drawn into and
//    PRESENTED to give it back. tests/render/pixel_tests.cpp draws fifty-one
//    frames and presents none of them - deliberately, because presenting a
//    flip-model swap chain discards what it wants to read - so a backend that
//    acquired per frame would block on the third one for ever. The seam permits
//    a frame that is submitted and never presented (renderer.h,
//    read_back_buffer); a swapchain image does not.
//  - A Win32 surface reports minImageExtent == maxImageExtent == the window's
//    client area, so a swapchain CANNOT be a size other than the window's.
//    renderer.h's back_buffer_size says both Direct3D backends "answer the size
//    they were told and let Present stretch", and that a drag-resize is exactly
//    when the two disagree. A swapchain cannot be in that state.
//  - VK_ERROR_OUT_OF_DATE_KHR comes back from acquire and from present with no
//    window message anywhere near it. docs/port/android.md calls this the term
//    the fifth backend exists to stress.
//
// SO THE COLOUR TARGET IS THE ENGINE'S AND THE SWAPCHAIN IS THE PRESENTATION
// ENGINE'S, and the frame ends with a blit from the first into the second.
// Every one of the three above dissolves: a frame that is never presented never
// touches a swapchain image; the colour target is exactly the size the shell
// said, so the stretch happens in the blit exactly as DXGI_SCALING_STRETCH does
// it next door; and out-of-date news is answered inside present() by remaking a
// swapchain, because what it invalidates is not what the seam calls the back
// buffer. That last is the answer to the question docs/port/android.md left
// open - the seam's resize contract has the right shape, and this backend is
// what tested it - and the cost is one full-screen blit per present, which for
// an engine that draws textured quads is not what a frame spends its time on.
//
// THE SYNCHRONISATION IS THE D3D12 ONE, SPELT DIFFERENTLY, AND THAT IS WHY THE
// FLOOR IS 1.2. A VkSemaphore of type TIMELINE is an ID3D12Fence: a monotonic
// counter the queue signals and the CPU waits on a value of. So this file has
// one counter, a value per frame in flight, and the same rule about who waits
// for what and when that engine/render/d3d12/device_resources.h states at
// length. Timeline semaphores are core in Vulkan 1.2 (2020); binary semaphores
// and fences would express the same thing in three objects and two rules, and
// the presentation engine still needs binary ones - which is why both kinds are
// below.

namespace labrador
{
	// The name of a VkResult, for a throw that has to say what went wrong (T6).
	// A result this backend cannot produce comes back as its number.
	std::string vk_result_name(VkResult result);

	// Throws std::runtime_error naming `what` and the result. `what` is the
	// call in the engine's words rather than the API's: "the swapchain could
	// not be created", not "vkCreateSwapchainKHR".
	void check_vk(VkResult result, const char* what);

	// Told when the device goes away and comes back.
	//
	// Named VulkanDeviceNotify rather than DeviceNotify, which is the seam's
	// (engine/render/renderer.h) and is what a game implements. Renderer::Impl
	// implements this one and forwards; nothing outside this folder sees it.
	class VulkanDeviceNotify
	{
	public:
		virtual void on_device_lost() = 0;
		virtual void on_device_restored() = 0;

	protected:
		~VulkanDeviceNotify() = default;
	};

	// The instance and the device, REFCOUNTED, and the refcount is the whole
	// reason this is a type rather than four members of DeviceResources.
	//
	// render_resources.h states as a term of the seam that a RenderResources
	// outlives the Renderer it was filled against, and closes with "nothing
	// here needs the renderer to still be alive: a texture releases itself, and
	// an ID3D12Resource holds its own reference on the device." The first half
	// is true here and the second is not: a VkImage is a handle with no
	// reference in it, and vkDestroyImage takes the VkDevice as its first
	// argument. A texture destroyed after the device would be a call into a
	// device that has gone, which is the one shape of undefined behaviour this
	// API gives no diagnostic for.
	//
	// So the reference D3D12 gets from COM is supplied here by hand: every
	// VulkanTexture holds a shared_ptr to this, and the device and the instance
	// outlive the last thing that names them. It costs one atomic per texture
	// per load and nothing at all per draw.
	class VulkanDevice
	{
	public:
		VulkanDevice() = default;
		~VulkanDevice();

		VulkanDevice(const VulkanDevice&) = delete;
		VulkanDevice& operator=(const VulkanDevice&) = delete;

		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		VkQueue queue = VK_NULL_HANDLE;
		uint32_t queue_family = 0;

		// Read once at device creation, because every allocation below asks
		// them and asking the driver is a call.
		VkPhysicalDeviceMemoryProperties memory_properties = {};

		// What a uniform buffer descriptor's offset must be a multiple of. The
		// transform ring pads to this, which is why a transform costs 256 bytes
		// on most drivers rather than the sixteen it is.
		VkDeviceSize uniform_alignment = 16;

		// Whether this device takes BC1/2/3. Asked at selection rather than at
		// the first texture, so a refusal can name the whole content set: 41 of
		// the 43 .dds this engine loads are block compressed and every font
		// atlas is (engine/render/texture_format.h).
		bool block_compression = false;

		// The index of a memory type that is in `allowed` and has every bit of
		// `wanted`. Throws std::runtime_error naming `what` when there is none,
		// which is a device that cannot hold the thing being made rather than a
		// mistake at the call site.
		uint32_t memory_type(uint32_t allowed, VkMemoryPropertyFlags wanted,
			const char* what) const;
	};

	// A buffer and the memory under it, which this API never pairs for you.
	//
	// Every other backend's buffer carries its own allocation - a
	// D3D12_HEAP_TYPE is a field of the resource description, a GL buffer's
	// store is glBufferData - and here they are two objects with two lifetimes.
	// Pairing them in one struct is what stops the second from being forgotten.
	struct VulkanBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize bytes = 0;

		// Non-null only for the host-visible ones, which are mapped once at
		// creation and never unmapped - the documented way to write from the
		// CPU, and what replaces D3D11's Map(DISCARD) for the reason
		// engine/render/d3d12/backend.h gives on VertexPage.
		void* mapped = nullptr;
	};

	VulkanBuffer create_vulkan_buffer(const VulkanDevice& owner,
		VkDeviceSize bytes, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memory, const char* what);

	void destroy_vulkan_buffer(VkDevice device, VulkanBuffer& buffer);

	class DeviceResources
	{
	public:
		// HOW MANY FRAMES THE CPU MAY RUN AHEAD. Two, for the reason
		// engine/render/d3d12/device_resources.h gives: three is the answer for
		// a title that wants the GPU never to starve, two is the answer for a
		// 2D sprite engine sized for the low tier, where the per-frame cost of
		// a third set is a command pool, a vertex buffer and a transform ring
		// nobody asked for.
		//
		// IT IS NOT THE SWAPCHAIN IMAGE COUNT HERE, WHERE ON D3D12 IT IS THE
		// SAME NUMBER. The presentation engine says how many images it has and
		// this backend takes what it is given, because the frame is not drawn
		// into one of them - see the top of this file.
		static const int FRAME_COUNT = 2;

		// Everything one frame in flight owns.
		struct Frame
		{
			// One pool, reset whole at the top of the frame rather than one
			// buffer freed at a time. That is what a pool is for, and it is
			// safe exactly because wait_for_frame has already run.
			VkCommandPool command_pool = VK_NULL_HANDLE;
			VkCommandBuffer commands = VK_NULL_HANDLE;

			// Signalled by the presentation engine when the image it handed out
			// is ready to be written. Per frame in flight, because a wait on it
			// is outstanding until the submit that consumes it finishes.
			VkSemaphore acquired = VK_NULL_HANDLE;

			// The vertices of every view of this frame, copied in at submit()
			// and grown rather than wrapped - the same answer d3d12's vertex
			// pages give, reached without pages because this backend builds the
			// whole frame's buffer in one place (renderer.cpp, submit).
			VulkanBuffer vertices;

			// One padded float4 per run, which is what the shader reads at b0.
			VulkanBuffer transforms;

			// Descriptor sets are allocated per run and thrown away with the
			// frame, so the pools are reset rather than freed. They grow: a
			// frame with more runs than the last takes another pool and keeps
			// it, which is cheaper than a cap nobody can pick correctly.
			std::vector<VkDescriptorPool> descriptor_pools;
			size_t descriptor_pool_in_use = 0;

			// How many sets have come out of the pool in use. Counted rather
			// than discovered from VK_ERROR_OUT_OF_POOL_MEMORY, because a loop
			// that reacts to a failure has to be sure the next attempt can
			// succeed - and a loop that takes a fresh pool every time one
			// refuses would spin for ever against a driver that refused for any
			// other reason. A pool holds a known number; this is how many of
			// them are gone (T3: take the simpler model).
			uint32_t descriptor_sets_taken = 0;

			// The timeline value that, once reached, means everything ever
			// submitted while this index was current has finished.
			uint64_t timeline_value = 0;
		};

		DeviceResources();
		~DeviceResources();

		DeviceResources(const DeviceResources&) = delete;
		DeviceResources& operator=(const DeviceResources&) = delete;
		DeviceResources(DeviceResources&&) = delete;
		DeviceResources& operator=(DeviceResources&&) = delete;

		// Called when the shell hands its window over, before anything else.
		void set_window(HWND window, int width, int height);

		void create_device_resources();
		void create_window_size_dependent_resources();

		// Whether anything was rebuilt, which is what the seam asks for.
		bool window_size_changed(int width, int height);

		void handle_device_lost();

		// Borrowed; Renderer::Impl owns this object and outlives it.
		void register_device_notify(VulkanDeviceNotify* notify)
		{
			this->notify_ = notify;
		}

		// --- The frame's command buffer --------------------------------------

		// The frame's command buffer, begun if it is not already recording.
		// Returns the same buffer either way, which is what makes it idempotent
		// WITHIN one operation: the barrier and the clear of a begin_frame are
		// two calls to this and one buffer, as are the transition, the copy and
		// the transition back of a read-back. It is not a way to add to what an
		// earlier entry point recorded - every entry point executes before it
		// returns, so there is never an open buffer between them.
		VkCommandBuffer commands();
		bool recording() const { return this->recording_; }

		// Ends and submits it, and moves the timeline on. Does nothing if
		// nothing opened it.
		//
		// `wait` and `signal` are the presentation engine's binary semaphores
		// and are null for every submit but the one that ends in a present -
		// which is the only submit that touches a swapchain image.
		//
		// ANSWERS FALSE WHEN THE DEVICE WAS LOST, having rebuilt everything on
		// the way out. That is not a courtesy: the caller that passes semaphores
		// is present(), and the semaphores it is about to hand vkQueuePresentKHR
		// belong to a swapchain the rebuild has destroyed. A present waiting on
		// a semaphore nothing will ever signal is a hang rather than an error.
		bool execute(VkSemaphore wait = VK_NULL_HANDLE,
			VkPipelineStageFlags wait_stage = 0,
			VkSemaphore signal = VK_NULL_HANDLE);

		// Throws away whatever the frame's command buffer holds without
		// executing any of it, by resetting the pool it was allocated from.
		// Two callers, and they want it for the same reason: a frame nobody
		// will submit (begin_frame) and a frame whose colour target is about to
		// be destroyed (window_size_changed).
		//
		// AND IT FORGETS THE TRACKED LAYOUT ONLY WHEN SOMETHING WAS ACTUALLY
		// RECORDED. The begin_frame caller runs every frame and usually
		// discards nothing, where the member is still true and throwing it away
		// makes the next frame's opening barrier order nothing at all. The
		// function says the whole of it.
		void abandon_commands();

		// Acquires a swapchain image, blits the colour target into it, presents
		// it, and moves to the next frame in flight. The whole of what this
		// backend does with a swapchain is in here.
		void present();

		// --- The timeline, and the whole of what this backend owes it ---------

		// Blocks until everything submitted against THIS frame index has
		// finished. Called once at the top of a frame, before anything resets a
		// command pool or overwrites a vertex buffer.
		void wait_for_frame();

		// Blocks until the queue has finished everything. NOT A FRAME-PATH
		// CALL: it is what a load, a read-back and a resize use, each of which
		// is already a stall by construction.
		void wait_for_gpu();

		// THE SAME WAIT, FOR THE PLACES WHERE THROWING DOES MORE DAMAGE THAN
		// THE FAILURE IT REPORTS - two destructors, which are implicitly
		// noexcept, and the resize path, which is reached from a window
		// procedure with nowhere above it to catch. Answers false instead.
		// engine/render/d3d12/device_resources.h carries the whole argument;
		// T6 is "not a licence for throwing on the way out".
		bool try_wait_for_gpu() noexcept;

		// --- What the renderer needs to know ----------------------------------

		const std::shared_ptr<VulkanDevice>& shared_device() const
		{
			return this->owner_;
		}

		VkDevice device() const
		{
			return this->owner_ ? this->owner_->device : VK_NULL_HANDLE;
		}

		VkQueue queue() const
		{
			return this->owner_ ? this->owner_->queue : VK_NULL_HANDLE;
		}

		const VulkanDevice& owner() const { return *this->owner_; }

		VkRenderPass render_pass() const { return this->render_pass_; }
		VkFramebuffer framebuffer() const { return this->framebuffer_; }
		VkImage colour_target() const { return this->colour_image_; }
		VkFormat colour_format() const { return this->colour_format_; }
		VkExtent2D colour_extent() const { return this->colour_extent_; }

		int frame_index() const { return this->frame_index_; }

		Frame& frame()
		{
			return this->frames_[static_cast<size_t>(this->frame_index_)];
		}

		RECT output_size() const { return this->output_size_; }

		// WHAT LAYOUT THE COLOUR TARGET IS IN, TRACKED RATHER THAN ASSUMED, and
		// it is the same member D3D12 keeps for the same reason: the one
		// resource whose state depends on what the client did rather than on
		// where in the frame we are. A frame that is read back leaves it having
		// been a transfer source, a frame that is presented leaves it having
		// been one too, and a frame that is neither leaves it a colour
		// attachment. A barrier from the wrong assumed layout is a validation
		// error on a good day and a corrupt frame on a bad one.
		// The stage and the access on BOTH sides come from the layouts, which
		// is why this takes one argument where a hand-written barrier takes
		// six. A barrier whose masks disagree with the layouts it names is the
		// class of mistake that produces a frame that is right on one driver
		// and torn on another, and there is one place here to get it right.
		void transition_colour(VkImageLayout layout);

		VkImageLayout colour_layout() const { return this->colour_layout_; }

		// The colour target has just been left in `layout` by a render pass
		// whose finalLayout says so, without a barrier this class recorded.
		void set_colour_layout(VkImageLayout layout)
		{
			this->colour_layout_ = layout;
		}

		// Room for `bytes` in this frame's vertex or transform buffer, taking a
		// bigger one if there is not. Called from submit(), which is single-
		// threaded and past wait_for_frame - the two facts that make destroying
		// the old one safe.
		void reserve_vertices(VkDeviceSize bytes);
		void reserve_transforms(VkDeviceSize bytes);

		// One descriptor set for one run, from this frame's pools. Takes
		// another pool when the current one is full.
		VkDescriptorSet allocate_descriptor_set(VkDescriptorSetLayout layout);

		// A command buffer for work that is not a frame's - a texture upload, a
		// buffer fill at device creation. Ended, submitted and waited for by
		// end_upload, because both callers are load paths and are stalls by
		// construction (engine/render/resource_factory.h).
		VkCommandBuffer begin_upload();
		void end_upload(VkCommandBuffer commands);

	private:
		void create_instance();
		void select_physical_device();
		void create_logical_device();
		void create_frames();
		void create_render_pass();
		void create_colour_target();

		// ANSWERS FALSE WHEN THE DEVICE WAS LOST, for the same reason execute()
		// does and only where there was a swapchain to replace. A minimised
		// window is not a failure: it answers true, having RELEASED the
		// swapchain rather than kept one it cannot use - which is what makes
		// present()'s null-swapchain branch reachable at all.
		bool create_swapchain();

		// Whether the surface has any area to present into, asked of the
		// surface rather than of the shell. It is how present() notices a
		// minimised window coming back, because no Win32 message says so:
		// engine/app/window.cpp's restore branch sends on_resuming and no size.
		bool surface_has_area() const noexcept;

		// The swapchain alone, waited for and remade. What present() does about
		// VK_ERROR_OUT_OF_DATE_KHR, and deliberately NOT
		// create_window_size_dependent_resources: the colour target is the
		// frame that was just drawn, and a presentation engine declaring its
		// own images stale is no reason to throw it away.
		//
		// Answers false on a device loss, having rebuilt everything - which
		// includes the command buffer of the frame the caller was presenting.
		bool rebuild_swapchain();

		void destroy_swapchain();
		void destroy_colour_target();
		void destroy_frames();

		std::shared_ptr<VulkanDevice> owner_;

		HWND window_ = nullptr;
		VkSurfaceKHR surface_ = VK_NULL_HANDLE;

		VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
		VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
		VkExtent2D swapchain_extent_ = { 0, 0 };
		std::vector<VkImage> swapchain_images_;

		// One per swapchain image rather than one per frame in flight, which is
		// the difference between a correct wait and a nearly correct one: a
		// present semaphore may not be reused until the present that waits on
		// it has finished with it, and the presentation engine decides which
		// image comes back when.
		std::vector<VkSemaphore> presentable_;

		// The image the frame is drawn into, which is not a swapchain image.
		// See the top of this file.
		VkImage colour_image_ = VK_NULL_HANDLE;
		VkDeviceMemory colour_memory_ = VK_NULL_HANDLE;
		VkImageView colour_view_ = VK_NULL_HANDLE;
		VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
		VkExtent2D colour_extent_ = { 0, 0 };
		VkImageLayout colour_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

		// B8G8R8A8_UNORM, which is what both Direct3D backends' back buffers
		// are and what a Win32 surface always offers. Matching them costs
		// nothing and keeps read_back_buffer's swizzle the same sentence in
		// three files.
		VkFormat colour_format_ = VK_FORMAT_B8G8R8A8_UNORM;

		VkRenderPass render_pass_ = VK_NULL_HANDLE;

		VkCommandPool upload_pool_ = VK_NULL_HANDLE;

		VkSemaphore timeline_ = VK_NULL_HANDLE;
		uint64_t timeline_value_ = 0;

		Frame frames_[FRAME_COUNT];
		int frame_index_ = 0;
		bool recording_ = false;

		// Whether this frame's command buffer has already been to the queue.
		//
		// ONE CALLER MAKES IT TRUE AND IT IS THE ONE THE SEAM NAMES.
		// read_back_buffer submits mid-frame and waits, because copying a GPU
		// image out is a stall by construction - and end_frame then wants the
		// same buffer again. A command buffer allocated from a pool without
		// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT may only be begun
		// from the INITIAL state, so reopening it after a submit is not a
		// begin: it is a wait and a pool reset first. commands() does both, and
		// this is how it knows to.
		bool submitted_ = false;

		RECT output_size_ = { 0, 0, 1, 1 };

		VulkanDeviceNotify* notify_ = nullptr;
	};
}

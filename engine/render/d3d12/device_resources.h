#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// The Direct3D 12 device, the queue, the swap chain and the fence.
//
// THE PART OF AN API THAT IS NOT ABOUT DRAWING, which is what renderer.h says
// a backend's fourth translation unit is for. The D3D11 and Vulkan folders
// have one of these too and the GL folder has gl_functions.cpp; only null
// stops at three files. This is the same kind of file as those three and holds
// the same kind of thing - everything that exists before a frame does and
// outlives every frame after it.
//
// IT IS NOT THE D3D11 FILE TRANSLITERATED, and the naming says so. That one is
// an upstream sample carried in with its own conventions - PascalCase methods,
// m_ members - and it is left as it is because rewriting a vendored file to
// match a house style is a diff nobody can review against its origin. This
// file is new, so it is written the way CONVENTIONS says: snake_case
// everything, trailing underscore on privates, accessors named for the noun.
// Two files with one name and two styles is worse than one style, and worse
// still is a third style invented to split the difference.
//
// WHAT IT DOES THAT THE D3D11 ONE DOES NOT, which is the whole reason this
// backend was worth writing: it owns synchronisation. D3D11 and OpenGL both
// hide the CPU/GPU boundary behind the driver - a MAP_DISCARD renames a buffer
// for you, a Present blocks when it must - and this API does not. So there is
// a fence in here, a value per frame in flight, and a rule about who waits for
// what and when. Everything above the seam is unchanged by that, which is the
// claim the fourth backend exists to test.

namespace labrador
{
	// Told when the device goes away and comes back.
	//
	// Named D3DDeviceNotify rather than DeviceNotify, which is the seam's
	// (engine/render/renderer.h) and is what a game implements. Renderer::Impl
	// implements this one and forwards; nothing outside this folder sees it.
	class D3DDeviceNotify
	{
	public:
		virtual void on_device_lost() = 0;
		virtual void on_device_restored() = 0;

	protected:
		~D3DDeviceNotify() = default;
	};

	class DeviceResources
	{
	public:
		// HOW MANY FRAMES THE CPU MAY RUN AHEAD, and it is the back buffer
		// count because those are the same number: a frame in flight is a back
		// buffer the GPU has not finished with, plus the per-frame recording
		// state that describes it.
		//
		// TWO, NOT THREE. Three is the usual answer for a title that wants the
		// GPU never to starve; two is the answer for a 2D sprite engine sized
		// for the low tier, where a frame is tens of draw calls and the per-
		// frame cost of a third set - a command allocator and a vertex page
		// per view - buys latency nobody asked for. The D3D11 backend's swap
		// chain has two buffers for the same reason and says nothing about it,
		// because there it is only a swap chain's business.
		//
		// PHILOSOPHY.md, Performance, names the low tier - and says that this
		// claim, unlike the vertex page's, is latency rather than a memory
		// budget, so it is one of the two waiting on a p99 nobody has taken.
		static const int FRAME_COUNT = 2;

		explicit DeviceResources(
			DXGI_FORMAT back_buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM);
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
		void register_device_notify(D3DDeviceNotify* notify)
		{
			this->notify_ = notify;
		}

		void present();

		// --- The fence, and the whole of what this backend owes it -----------

		// Records that everything submitted up to now belongs to this frame
		// index, so the next pass over the same index knows what to wait for.
		// Called after every ExecuteCommandLists.
		//
		// ONE COUNTER, SIGNALLED AFTER EVERY EXECUTE, rather than the sample's
		// signal-only-at-present. The difference matters because a client that
		// never presents is not hypothetical: tests/render/pixel_tests.cpp
		// draws, submits and reads the buffer back without ever presenting,
		// deliberately, because presenting a flip-model swap chain discards
		// what it wants to read. Under signal-at-present, such a client resets
		// a command allocator the GPU may still be reading from, and the only
		// symptom would be a debug-layer message the preset that runs those
		// tests does not always have.
		//
		// THE RULE IS RIGHT AND pixel_tests IS NOT THE PROOF OF IT, which is
		// worth being exact about because that file was cited here as though it
		// were. Every frame termination in it routes through read_back_buffer,
		// which ends in wait_for_gpu below, so the hazard is live at zero frame
		// boundaries there. The client this protects is the one that draws,
		// submits and does not present WITHOUT reading back - which this seam
		// permits, nothing here forbids, and no test currently is.
		void signal_frame();

		// Blocks until the work signal_frame recorded for THIS frame index has
		// finished. Called once at the top of a frame, before anything resets a
		// command allocator - which is the one rule this API has that D3D11, GL
		// and null do not. Vulkan has it identically, declared in the identical
		// place (vulkan/device_resources.h, wait_for_frame) with a command pool
		// where this says allocator, because a timeline semaphore is an
		// ID3D12Fence spelt differently.
		void wait_for_frame();

		// Blocks until the GPU has finished everything. NOT A FRAME-PATH CALL:
		// it is what a load, a read-back and device creation use, each of which
		// is already a stall by construction. Throws com_exception if the queue
		// will not take the signal, which is a removed or reset device and
		// nothing else.
		void wait_for_gpu();

		// THE SAME WAIT, FOR THE THREE PLACES WHERE THROWING DOES MORE DAMAGE
		// THAN THE FAILURE IT REPORTS. Answers false instead, having waited for
		// nothing.
		//
		// Two of them are destructors - this class's and Renderer::Impl's -
		// which are implicitly noexcept because every member of both has a
		// non-throwing destructor, so a throw out of either is std::terminate
		// and the GPU wait they exist for never happens. PHILOSOPHY.md says it
		// by name: T6 is "not a licence for throwing on the way out - teardown
		// stays silent".
		//
		// The third is create_window_size_dependent_resources, where the wait
		// stands fifteen lines above the DXGI_ERROR_DEVICE_REMOVED branch that
		// is this backend's only device-loss recovery outside present(). A
		// device removed while the window is being dragged reaches that wait
		// first, and a throw there unwinds out of a window procedure into
		// DispatchMessage, which is the "nowhere to catch" renderer.h names.
		// The recovery has to be reachable in exactly the ordering it was
		// written for.
		bool try_wait_for_gpu() noexcept;

		// After a present: the swap chain decides which buffer is next.
		void move_to_next_frame();

		// --- What the renderer needs to know about the device ----------------

		ID3D12Device* device() const { return this->device_.Get(); }
		ID3D12CommandQueue* command_queue() const
		{
			return this->command_queue_.Get();
		}

		int frame_index() const { return this->frame_index_; }

		ID3D12Resource* back_buffer() const
		{
			return this->back_buffers_[
				static_cast<size_t>(this->frame_index_)].Get();
		}

		D3D12_CPU_DESCRIPTOR_HANDLE back_buffer_view() const;

		// WHAT STATE THE BACK BUFFER IS IN, TRACKED RATHER THAN ASSUMED.
		//
		// D3D12 makes the caller name every resource transition, and the back
		// buffer is the one resource whose state depends on what the client did
		// rather than on where in the frame we are: a frame that is presented
		// leaves it in PRESENT, a frame that is read back leaves it in
		// RENDER_TARGET, and both are ordinary (see signal_frame above for who
		// does the second). A barrier issued from the wrong assumed state is a
		// debug-layer error on a good day and a corrupt frame on a bad one, so
		// the state is a member and every transition goes through it.
		D3D12_RESOURCE_STATES back_buffer_state() const;
		void set_back_buffer_state(D3D12_RESOURCE_STATES state);

		RECT output_size() const { return this->output_size_; }
		D3D12_VIEWPORT screen_viewport() const { return this->screen_viewport_; }
		DXGI_FORMAT back_buffer_format() const
		{
			return this->back_buffer_format_;
		}

	private:
		void create_factory();
		void hardware_adapter(IDXGIAdapter1** adapter);

		// The two halves of a fence wait, as HRESULTs rather than as throws.
		// Everything above that signals or waits is one of these two reported
		// one of two ways, so there is one copy of each and no path where a
		// throwing form and an answering form can drift apart.
		HRESULT record_signal() noexcept;
		HRESULT block_until(UINT64 target) noexcept;

		Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
		Microsoft::WRL::ComPtr<ID3D12Device> device_;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain_;

		Microsoft::WRL::ComPtr<ID3D12Resource> back_buffers_[FRAME_COUNT];
		D3D12_RESOURCE_STATES back_buffer_states_[FRAME_COUNT];

		// Non shader-visible: a render target view is only ever read by the
		// command list that binds it.
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> render_target_heap_;
		UINT render_target_size_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		HANDLE fence_event_ = nullptr;

		// Monotonic. `frame_fences_[i]` is the value that, once reached, means
		// everything ever submitted against frame index i has finished.
		UINT64 fence_value_ = 0;
		UINT64 frame_fences_[FRAME_COUNT];

		int frame_index_ = 0;

		DXGI_FORMAT back_buffer_format_;
		D3D12_VIEWPORT screen_viewport_;
		RECT output_size_;
		HWND window_ = nullptr;

		D3DDeviceNotify* notify_ = nullptr;
	};
}

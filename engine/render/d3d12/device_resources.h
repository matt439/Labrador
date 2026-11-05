#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// The Direct3D 12 device, the queue, the swap chain and the fence.
//
// Owns the device resources shared by frames and the fence values that guard
// per-frame allocator and vertex-memory reuse. Drawing and command recording
// remain in renderer.cpp behind engine/render/renderer.h's neutral seam.

namespace labrador
{
	// Backend device-loss callbacks. Renderer::Impl forwards them to the
	// neutral DeviceNotify in engine/render/renderer.h.
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
		// Back-buffer and per-frame recording-slot count. Reusing a slot waits
		// for its submitted work. This bounds storage, not measured pipeline
		// latency; docs/performance/2026-09-19-g6f-ratchet.md#5 owns that evidence.
		static const int frame_count = 2;

		// The swap chain's format, which the render target views and the
		// pipeline state use too. Renderer::read_back_buffer swaps B and R
		// to answer RGBA, so it static_asserts this rather than trusting it.
		static constexpr DXGI_FORMAT back_buffer_format =
			DXGI_FORMAT_B8G8R8A8_UNORM;

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

		// Whether the swap chain is built at this size, which is the question
		// window_size_changed asks before rebuilding. False after a rebuild
		// that threw, whatever the size, so that the next call rebuilds.
		bool has_size(int width, int height) const;

		// Borrowed callback target; it must outlive its registration here.
		void register_device_notify(D3DDeviceNotify* notify)
		{
			this->notify_ = notify;
		}

		// The sync interval is Renderer::set_vsync's answer, 1 or 0.
		void present(UINT sync_interval);

		// --- The fence, and the whole of what this backend owes it -----------

		// Signal the queue after every ExecuteCommandLists and associate the
		// value with this frame index. Slot reuse must be safe even when a
		// caller submits without presenting or draining through read-back.
		// A read-back calls wait_for_gpu(), so it cannot by itself demonstrate
		// a pending slot-reuse wait. docs/review/INDEX.md owns lifecycle evidence.
		void signal_frame();

		// Wait for this frame index's recorded fence target before resetting
		// its allocators or overwriting its vertex pages. Called by begin_frame.
		void wait_for_frame();

		// Signal and wait for all previously queued work. Used for resource
		// upload, read-back and creation rather than ordinary frame-slot reuse.
		// Throws com_exception when signaling or arranging completion fails, or
		// the wait itself fails: the device-removed reason when the device has
		// gone, and otherwise the failing call's HRESULT.
		void wait_for_gpu();

		// The non-throwing form for teardown (T6). Returns false if signaling
		// or arranging completion fails, or the wait itself fails; callers must
		// not treat that as completed work. Returns true without waiting when
		// queue, fence or event resources are absent.
		bool try_wait_for_gpu() noexcept;

		// --- What the renderer needs to know about the device ----------------

		ID3D12Device* device() const { return this->device_.Get(); }

		// The description of the adapter create_device_resources made the
		// device on: the hardware adapter it chose, or WARP.
		const DXGI_ADAPTER_DESC1& adapter_description() const
		{
			return this->adapter_description_;
		}

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

		// Tracked state of the current back buffer, used as each barrier's
		// source state. Read-back restores the prior state; end_frame transitions
		// to PRESENT. A submitted frame need not be presented, so frame position
		// alone does not determine the resource state.
		D3D12_RESOURCE_STATES back_buffer_state() const;
		void set_back_buffer_state(D3D12_RESOURCE_STATES state);

		RECT output_size() const { return this->output_size_; }
		D3D12_VIEWPORT screen_viewport() const { return this->screen_viewport_; }

	private:
		// Rebuilds everything after a removal or reset that present or a
		// resize found, between the notify's two callbacks.
		void handle_device_lost();

		// After a present: the swap chain decides which buffer is next.
		void move_to_next_frame();

		void create_factory();

		// The first adapter that is not software and can make a Direct3D 12
		// device at this backend's floor, or null when there is none.
		Microsoft::WRL::ComPtr<IDXGIAdapter1> hardware_adapter();

		// Shared signaling and completion operations for the throwing and
		// non-throwing wait entry points.
		HRESULT record_signal() noexcept;
		HRESULT block_until(UINT64 target) noexcept;

		// Signal and wait for all queued work, answering the failing call's
		// HRESULT, or S_OK without waiting when queue, fence or event resources
		// are absent. Resize calls it directly so that it can reach its
		// device-removal handling instead of unwinding through window dispatch,
		// and still report a live device's failure by its own code.
		HRESULT signal_and_wait() noexcept;

		Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
		Microsoft::WRL::ComPtr<ID3D12Device> device_;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain_;

		DXGI_ADAPTER_DESC1 adapter_description_ = {};

		Microsoft::WRL::ComPtr<ID3D12Resource> back_buffers_[frame_count];
		D3D12_RESOURCE_STATES back_buffer_states_[frame_count];

		// Non shader-visible: a render target view is only ever read by the
		// command list that binds it.
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> render_target_heap_;
		UINT render_target_size_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		Microsoft::WRL::Wrappers::Event fence_event_;

		// Monotonic. `frame_fences_[i]` is the value that, once reached, means
		// everything ever submitted against frame index i has finished.
		UINT64 fence_value_ = 0;
		UINT64 frame_fences_[frame_count];

		int frame_index_ = 0;

		D3D12_VIEWPORT screen_viewport_;
		RECT output_size_;

		// Set while a rebuild at output_size_ has not completed.
		bool size_stale_ = false;
		HWND window_ = nullptr;

		D3DDeviceNotify* notify_ = nullptr;
	};
}

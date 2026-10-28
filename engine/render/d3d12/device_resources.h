#pragma once

#include <Windows.h>
#include <wrl/client.h>
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

		// Borrowed callback target; it must outlive its registration here.
		void register_device_notify(D3DDeviceNotify* notify)
		{
			this->notify_ = notify;
		}

		void present();

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
		// Throws com_exception when signaling or arranging completion fails.
		void wait_for_gpu();

		// The non-throwing form for teardown (T6) and resize recovery. Returns
		// false if signaling or arranging completion fails; callers must not
		// treat that as completed work. Returns true without waiting when queue,
		// fence or event resources are absent. Resize can then reach its
		// device-removal handling instead of unwinding through window dispatch.
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

		// Tracked state of the current back buffer, used as each barrier's
		// source state. Read-back restores the prior state; end_frame transitions
		// to PRESENT. A submitted frame need not be presented, so frame position
		// alone does not determine the resource state.
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

		// Shared signaling and completion operations for the throwing and
		// non-throwing wait entry points.
		HRESULT record_signal() noexcept;
		HRESULT block_until(UINT64 target) noexcept;

		Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
		Microsoft::WRL::ComPtr<ID3D12Device> device_;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain_;

		Microsoft::WRL::ComPtr<ID3D12Resource> back_buffers_[frame_count];
		D3D12_RESOURCE_STATES back_buffer_states_[frame_count];

		// Non shader-visible: a render target view is only ever read by the
		// command list that binds it.
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> render_target_heap_;
		UINT render_target_size_ = 0;

		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		HANDLE fence_event_ = nullptr;

		// Monotonic. `frame_fences_[i]` is the value that, once reached, means
		// everything ever submitted against frame index i has finished.
		UINT64 fence_value_ = 0;
		UINT64 frame_fences_[frame_count];

		int frame_index_ = 0;

		DXGI_FORMAT back_buffer_format_;
		D3D12_VIEWPORT screen_viewport_;
		RECT output_size_;
		HWND window_ = nullptr;

		D3DDeviceNotify* notify_ = nullptr;
	};
}

#include "engine/render/d3d12/device_resources.h"

#include "engine/render/throw_if_failed.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <tuple>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

using Microsoft::WRL::ComPtr;

namespace labrador
{
	namespace
	{
		// THE FLOOR, AND IT IS HIGHER THAN THE ENGINE'S.
		//
		// engine/render/d3d11/device_resources.h puts this engine's floor at
		// feature level 10.0. Direct3D 12 has no such level: 11_0 is the lowest
		// there is, and the OS floor is Windows 10. So this backend reaches
		// strictly less hardware than the one beside it, which is worth stating
		// plainly rather than leaving to be discovered - it is here to hold the
		// seam to an API where the engine owns synchronisation, and to give the
		// pixel contract a second rasteriser CI can actually run.
		//
		// AND "NOT A TARGET ON THE LOW TIER" IS NO LONGER THE RIGHT SENTENCE,
		// which is what naming the low tier cost this comment. PHILOSOPHY.md,
		// Performance names it - the Radeon iGPU in a Ryzen 9000 desktop
		// package, at 1280x720, held to four cores - and that part is feature
		// level 12_2 with a current driver, so this backend runs there as
		// happily as the one next door. What the floor above actually excludes
		// is hardware OLDER than the named configuration: a feature-level-10
		// part, or a Haswell-era one whose vendor never shipped a D3D12 driver
		// (docs/survey/2026-08-26-status.md's 2013 MacBook Air is exactly that
		// machine, and it runs the D3D11 and GL backends and neither of the
		// other two). That is a claim about the successor machine rather than
		// about the reference one, and it is the only NEGATIVE low-tier
		// sentence in the tree - which is why it is worth being exact in.
		const D3D_FEATURE_LEVEL MIN_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;
	}

	DeviceResources::DeviceResources(DXGI_FORMAT back_buffer_format)
		: back_buffer_states_{},
		  frame_fences_{},
		  back_buffer_format_(back_buffer_format),
		  screen_viewport_{},
		  output_size_{ 0, 0, 1, 1 }
	{
		for (int i = 0; i < FRAME_COUNT; i++)
		{
			this->back_buffer_states_[i] = D3D12_RESOURCE_STATE_PRESENT;
			this->frame_fences_[i] = 0;
		}
	}

	DeviceResources::~DeviceResources()
	{
		// ONE OF THE FOUR DESTRUCTORS IN engine/render/ THAT HAVE TO
		// SYNCHRONISE WITH A GPU, and it is the same fact this whole file
		// exists for. Releasing a D3D11 device is enough because the runtime
		// tracks what is still in flight; here, dropping a command list the GPU
		// is still reading is exactly the bug the fence exists to prevent, and
		// a destructor is no exception. The second is ~Renderer::Impl in
		// renderer.cpp, from the same commit and for the same reason, and it
		// covers the lists, the allocators and the vertex pages this class does
		// not own. The other two are the Vulkan backend's pair of the same
		// shape, which arrived with the fifth backend - a timeline semaphore
		// being an ID3D12Fence spelt differently, its two destructors are this
		// pair spelt differently too.
		//
		// AND IT ANSWERS RATHER THAN THROWS. This destructor is implicitly
		// noexcept - every member has a non-throwing one - so a com_exception
		// out of the wait would be std::terminate on an ordinary exit, and the
		// wait it was raised from would not have happened either way. T6:
		// teardown stays silent.
		std::ignore = this->try_wait_for_gpu();

		if (this->fence_event_ != nullptr)
		{
			CloseHandle(this->fence_event_);
			this->fence_event_ = nullptr;
		}
	}

	void DeviceResources::set_window(HWND window, int width, int height)
	{
		this->window_ = window;

		this->output_size_.left = 0;
		this->output_size_.top = 0;
		this->output_size_.right = static_cast<long>(width);
		this->output_size_.bottom = static_cast<long>(height);
	}

	void DeviceResources::create_factory()
	{
		UINT flags = 0;

#if defined(_DEBUG)
		// The debug layer is an optional Windows feature ("Graphics Tools"),
		// so this asks and carries on. A build machine without it is the
		// normal case and is not a reason to refuse to run - which is the
		// same shape the D3D11 file's SdkLayersAvailable has.
		{
			ComPtr<ID3D12Debug> debug;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(
				debug.GetAddressOf()))))
			{
				debug->EnableDebugLayer();
			}
			else
			{
				OutputDebugStringA(
					"WARNING: Direct3D 12 debug layer is not available\n");
			}

			ComPtr<IDXGIInfoQueue> dxgi_info;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(
				dxgi_info.GetAddressOf()))))
			{
				flags = DXGI_CREATE_FACTORY_DEBUG;

				dxgi_info->SetBreakOnSeverity(DXGI_DEBUG_ALL,
					DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
				dxgi_info->SetBreakOnSeverity(DXGI_DEBUG_ALL,
					DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
			}
		}
#endif

		ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(
			this->factory_.ReleaseAndGetAddressOf())));
	}

	void DeviceResources::hardware_adapter(IDXGIAdapter1** adapter)
	{
		*adapter = nullptr;

		ComPtr<IDXGIAdapter1> candidate;

		ComPtr<IDXGIFactory6> factory6;
		if (SUCCEEDED(this->factory_.As(&factory6)))
		{
			for (UINT index = 0;
				SUCCEEDED(factory6->EnumAdapterByGpuPreference(index,
					DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
					IID_PPV_ARGS(candidate.ReleaseAndGetAddressOf())));
				index++)
			{
				DXGI_ADAPTER_DESC1 description;
				ThrowIfFailed(candidate->GetDesc1(&description));

				if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					// The Basic Render Driver, which is not what the fallback
					// below means by WARP and is not wanted here either.
					continue;
				}

				// ASKED WHETHER IT CAN MAKE A DEVICE, WHICH THE D3D11 FILE DOES
				// NOT HAVE TO. There, every adapter DXGI enumerates supports
				// D3D11 at some level; here an adapter can be present and not
				// support Direct3D 12 at all.
				//
				// WHAT IT BUYS IS THE SECOND ADAPTER, NOT THE FALLBACK. Without
				// the probe this loop takes the first non-software adapter and
				// stops, so a machine whose first adapter cannot do D3D12 and
				// whose second can would drop to WARP - the fallback below is a
				// plain if rather than the else of `if (candidate)`, so it
				// catches that case either way and nothing throws. With the
				// probe the loop keeps looking, and the capable GPU is used.
				if (SUCCEEDED(D3D12CreateDevice(candidate.Get(),
					MIN_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr)))
				{
					break;
				}
			}
		}

		if (!candidate)
		{
			for (UINT index = 0;
				SUCCEEDED(this->factory_->EnumAdapters1(index,
					candidate.ReleaseAndGetAddressOf()));
				index++)
			{
				DXGI_ADAPTER_DESC1 description;
				ThrowIfFailed(candidate->GetDesc1(&description));

				if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				if (SUCCEEDED(D3D12CreateDevice(candidate.Get(),
					MIN_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr)))
				{
					break;
				}
			}
		}

		*adapter = candidate.Detach();
	}

	void DeviceResources::create_device_resources()
	{
		this->create_factory();

		ComPtr<IDXGIAdapter1> adapter;
		this->hardware_adapter(adapter.GetAddressOf());

		HRESULT hr = E_FAIL;
		if (adapter)
		{
			hr = D3D12CreateDevice(adapter.Get(), MIN_FEATURE_LEVEL,
				IID_PPV_ARGS(this->device_.ReleaseAndGetAddressOf()));
		}
#if defined(NDEBUG)
		else
		{
			throw std::runtime_error("No Direct3D 12 hardware device found");
		}
#else
		if (FAILED(hr))
		{
			// WARP, AND THIS LINE IS WHY THIS BACKEND IS THE FOURTH ONE.
			//
			// A GitHub runner has no GPU. The OpenGL backend's answer to that
			// is that .github/workflows/ci.yml skips RenderPixelTests for it
			// entirely, because Windows' fallback for OpenGL is GDI 1.1 and
			// create_device throws. Direct3D has an in-box, fully conformant
			// software rasteriser instead, and Direct3D 12 has it too - so this
			// is the second backend whose pixel contract CI can actually check,
			// against the same golden images as the first.
			ComPtr<IDXGIAdapter> warp;
			if (SUCCEEDED(this->factory_->EnumWarpAdapter(
				IID_PPV_ARGS(warp.GetAddressOf()))))
			{
				hr = D3D12CreateDevice(warp.Get(), MIN_FEATURE_LEVEL,
					IID_PPV_ARGS(this->device_.ReleaseAndGetAddressOf()));

				if (SUCCEEDED(hr))
				{
					OutputDebugStringA("Direct3D 12 Adapter - WARP\n");
				}
			}
		}
#endif

		ThrowIfFailed(hr);

#ifndef NDEBUG
		{
			ComPtr<ID3D12InfoQueue> info_queue;
			if (SUCCEEDED(this->device_.As(&info_queue)))
			{
				info_queue->SetBreakOnSeverity(
					D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,
					TRUE);
			}
		}
#endif

		// ONE QUEUE, AND THE SEAM WILL NEVER ASK FOR A SECOND. The parallelism
		// axis is views; views record in parallel and are executed in view
		// order, which is one ExecuteCommandLists on one queue. A copy queue
		// for the load path would be a second thing to synchronise for work
		// that already stalls - texture_factory.cpp beside this file is where
		// that stall is argued, and resource_factory.h is where the seam says
		// loading is synchronous at all.
		D3D12_COMMAND_QUEUE_DESC queue_description = {};
		queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queue_description.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ThrowIfFailed(this->device_->CreateCommandQueue(&queue_description,
			IID_PPV_ARGS(this->command_queue_.ReleaseAndGetAddressOf())));

		D3D12_DESCRIPTOR_HEAP_DESC heap_description = {};
		heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heap_description.NumDescriptors = FRAME_COUNT;
		heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(this->device_->CreateDescriptorHeap(&heap_description,
			IID_PPV_ARGS(this->render_target_heap_.ReleaseAndGetAddressOf())));

		this->render_target_size_ =
			this->device_->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		ThrowIfFailed(this->device_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(this->fence_.ReleaseAndGetAddressOf())));

		this->fence_value_ = 0;
		for (int i = 0; i < FRAME_COUNT; i++)
		{
			this->frame_fences_[i] = 0;
		}

		if (this->fence_event_ == nullptr)
		{
			this->fence_event_ = CreateEventEx(nullptr, nullptr, 0,
				EVENT_MODIFY_STATE | SYNCHRONIZE);
			if (this->fence_event_ == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}
	}

	void DeviceResources::create_window_size_dependent_resources()
	{
		if (this->window_ == nullptr)
		{
			throw std::logic_error(
				"Call set_window with a valid Win32 window handle");
		}

		// Nothing may be released while the GPU is still reading it, and a back
		// buffer is the thing most likely to be.
		//
		// AND IT ANSWERS RATHER THAN THROWS, because the branch that recovers
		// from a device removal is fifteen lines below this one. A TDR or a
		// driver update lands while a window is being dragged, WM_EXITSIZEMOVE
		// arrives with the removal still clearing, and this wait is the first
		// thing to touch D3D afterwards - so a throw here skipped
		// handle_device_lost in exactly the ordering it was written for and
		// unwound into DispatchMessage instead, where the shell has nowhere to
		// catch it. Same recovery, reached from the wait as well as from
		// ResizeBuffers.
		//
		// AND WHAT MAKES IT A LOSS IS THE DEVICE SAYING SO, NOT THE WAIT
		// ANSWERING FALSE. try_wait_for_gpu answers false for any failed
		// Signal or SetEventOnCompletion, and an E_OUTOFMEMORY out of either
		// is not a removal - it is a live device with work still executing on
		// it. Recovering there ran handle_device_lost against exactly that:
		// its first statement tells the client the device is gone and its next
		// six release the queue, the fence and the device itself while the GPU
		// is reading them. This is the ordinary WM_EXITSIZEMOVE path, not a
		// rare one. So the device is asked, which is the same question
		// wait_for_gpu asks below and the same E_FAIL for the answer that
		// cannot be explained - and a throw is right here where it is wrong
		// above, because there is no recovery being skipped: nothing has been
		// lost, and continuing would release a back buffer the GPU still has.
		if (!this->try_wait_for_gpu())
		{
			const HRESULT reason = this->device_->GetDeviceRemovedReason();
			if (FAILED(reason))
			{
				this->handle_device_lost();
				return;
			}

			ThrowIfFailed(E_FAIL);
		}

		for (int i = 0; i < FRAME_COUNT; i++)
		{
			this->back_buffers_[i].Reset();
			this->back_buffer_states_[i] = D3D12_RESOURCE_STATE_PRESENT;
		}

		const UINT width = std::max<UINT>(static_cast<UINT>(
			this->output_size_.right - this->output_size_.left), 1u);
		const UINT height = std::max<UINT>(static_cast<UINT>(
			this->output_size_.bottom - this->output_size_.top), 1u);

		if (this->swap_chain_)
		{
			const HRESULT hr = this->swap_chain_->ResizeBuffers(FRAME_COUNT,
				width, height, this->back_buffer_format_, 0u);

			if (hr == DXGI_ERROR_DEVICE_REMOVED ||
				hr == DXGI_ERROR_DEVICE_RESET)
			{
				// handle_device_lost re-enters this function with a new device,
				// so this one must not continue with the old one's state.
				this->handle_device_lost();
				return;
			}

			ThrowIfFailed(hr);
		}
		else
		{
			// FLIP_DISCARD IS NOT OPTIONAL HERE, WHERE IT IS A CHOICE NEXT
			// DOOR. Direct3D 12 has no bitblt swap effect at all - the
			// DISCARD/SEQUENTIAL models are D3D11 and earlier - so the
			// flip-model question the D3D11 file carries a runtime probe for
			// does not exist in this one. Everything that follows from flip is
			// therefore unconditional: no sRGB back buffer format, and a
			// present that discards what it presented, which is why
			// tests/render/pixel_tests.cpp calls end_frame exactly once - in
			// the one case that exists to walk the far end of
			// read_back_buffer's interval, after it has already read.
			DXGI_SWAP_CHAIN_DESC1 description = {};
			description.Width = width;
			description.Height = height;
			description.Format = this->back_buffer_format_;
			description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			description.BufferCount = FRAME_COUNT;
			description.SampleDesc.Count = 1;
			description.SampleDesc.Quality = 0;
			description.Scaling = DXGI_SCALING_STRETCH;
			description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			description.Flags = 0u;

			DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen = {};
			fullscreen.Windowed = TRUE;

			// FROM THE QUEUE, NOT FROM THE DEVICE, which is the one line of
			// this function a reader coming from the D3D11 file will trip over:
			// a D3D12 swap chain is created against the command queue that will
			// present from it, because presenting is queued work like any
			// other.
			ComPtr<IDXGISwapChain1> swap_chain;
			ThrowIfFailed(this->factory_->CreateSwapChainForHwnd(
				this->command_queue_.Get(), this->window_, &description,
				&fullscreen, nullptr, swap_chain.GetAddressOf()));

			ThrowIfFailed(swap_chain.As(&this->swap_chain_));

			// No exclusive full screen, and therefore no ALT+ENTER.
			ThrowIfFailed(this->factory_->MakeWindowAssociation(this->window_,
				DXGI_MWA_NO_ALT_ENTER));
		}

		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			this->render_target_heap_->GetCPUDescriptorHandleForHeapStart();

		D3D12_RENDER_TARGET_VIEW_DESC view_description = {};
		view_description.Format = this->back_buffer_format_;
		view_description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		for (int i = 0; i < FRAME_COUNT; i++)
		{
			ThrowIfFailed(this->swap_chain_->GetBuffer(static_cast<UINT>(i),
				IID_PPV_ARGS(this->back_buffers_[i].ReleaseAndGetAddressOf())));

			this->device_->CreateRenderTargetView(this->back_buffers_[i].Get(),
				&view_description, handle);
			handle.ptr += this->render_target_size_;
		}

		this->frame_index_ = static_cast<int>(
			this->swap_chain_->GetCurrentBackBufferIndex());

		this->screen_viewport_ = { 0.0f, 0.0f, static_cast<float>(width),
			static_cast<float>(height), 0.0f, 1.0f };
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
			this->notify_->on_device_lost();
		}

		for (int i = 0; i < FRAME_COUNT; i++)
		{
			this->back_buffers_[i].Reset();
			this->back_buffer_states_[i] = D3D12_RESOURCE_STATE_PRESENT;
		}

		this->render_target_heap_.Reset();
		this->swap_chain_.Reset();
		this->fence_.Reset();
		this->command_queue_.Reset();
		this->device_.Reset();
		this->factory_.Reset();

		this->create_device_resources();
		this->create_window_size_dependent_resources();

		if (this->notify_ != nullptr)
		{
			this->notify_->on_device_restored();
		}
	}

	void DeviceResources::present()
	{
		// Vsync-locked, like the backend next door: nothing in this tree sets a
		// tearing flag, so there is no capability probe here to measure one.
		const HRESULT hr = this->swap_chain_->Present(1, 0);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
#ifdef _DEBUG
			char message[64] = {};
			sprintf_s(message, "Device lost on Present: reason 0x%08X\n",
				static_cast<unsigned int>(hr == DXGI_ERROR_DEVICE_REMOVED
					? this->device_->GetDeviceRemovedReason() : hr));
			OutputDebugStringA(message);
#endif
			this->handle_device_lost();
			return;
		}

		ThrowIfFailed(hr);

		this->move_to_next_frame();
	}

	HRESULT DeviceResources::record_signal() noexcept
	{
		this->fence_value_++;

		const HRESULT hr = this->command_queue_->Signal(this->fence_.Get(),
			this->fence_value_);
		if (FAILED(hr))
		{
			return hr;
		}

		this->frame_fences_[static_cast<size_t>(this->frame_index_)] =
			this->fence_value_;
		return S_OK;
	}

	HRESULT DeviceResources::block_until(UINT64 target) noexcept
	{
		// AND A REMOVED DEVICE CANNOT HANG HERE, which is worth knowing before
		// reading the INFINITE below: the runtime signals every fence to
		// UINT64_MAX when a device is removed, so this early-out is what a
		// removal takes rather than the wait.
		if (this->fence_->GetCompletedValue() >= target)
		{
			return S_OK;
		}

		const HRESULT hr = this->fence_->SetEventOnCompletion(target,
			this->fence_event_);
		if (FAILED(hr))
		{
			return hr;
		}

		WaitForSingleObjectEx(this->fence_event_, INFINITE, FALSE);
		return S_OK;
	}

	void DeviceResources::signal_frame()
	{
		ThrowIfFailed(this->record_signal());
	}

	void DeviceResources::wait_for_frame()
	{
		ThrowIfFailed(this->block_until(
			this->frame_fences_[static_cast<size_t>(this->frame_index_)]));
	}

	void DeviceResources::wait_for_gpu()
	{
		if (this->try_wait_for_gpu())
		{
			return;
		}

		// The only way the wait above fails is a device that has gone, so what
		// this reports is the removal rather than the call that noticed it.
		// E_FAIL stands in for the case that cannot be explained that way,
		// because ThrowIfFailed(S_OK) would be a silent return.
		const HRESULT reason = this->device_->GetDeviceRemovedReason();
		ThrowIfFailed(FAILED(reason) ? reason : E_FAIL);
	}

	bool DeviceResources::try_wait_for_gpu() noexcept
	{
		if (!this->command_queue_ || !this->fence_ ||
			this->fence_event_ == nullptr)
		{
			return true;
		}

		if (FAILED(this->record_signal()))
		{
			return false;
		}

		return SUCCEEDED(this->block_until(
			this->frame_fences_[static_cast<size_t>(this->frame_index_)]));
	}

	void DeviceResources::move_to_next_frame()
	{
		// The signal was already recorded by whoever executed this frame's
		// lists; presenting does not add work of its own that anything waits
		// for separately. What changes here is only which buffer is current -
		// and it is asked of the swap chain rather than derived by adding one,
		// because a flip-model swap chain is entitled to disagree.
		this->frame_index_ = static_cast<int>(
			this->swap_chain_->GetCurrentBackBufferIndex());
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::back_buffer_view() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			this->render_target_heap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(this->frame_index_) *
			this->render_target_size_;
		return handle;
	}

	D3D12_RESOURCE_STATES DeviceResources::back_buffer_state() const
	{
		return this->back_buffer_states_[
			static_cast<size_t>(this->frame_index_)];
	}

	void DeviceResources::set_back_buffer_state(D3D12_RESOURCE_STATES state)
	{
		this->back_buffer_states_[static_cast<size_t>(this->frame_index_)] =
			state;
	}
}

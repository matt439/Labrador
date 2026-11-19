#pragma once

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <atomic>
#include <cwchar>
#include <new>
#include <stdexcept>
#include <utility>

// A scoped view of the real factory's adapter list. The factory and WARP
// adapter are actual DXGI objects; only the two enumeration answers change.
namespace render_tests::dxgi_factory
{
	enum class Mode
	{
		no_devices,
		software_only,
		hardware_control,
		unflagged_software_ids,
		named_software,
		software_then_hardware,
		error
	};

	struct Probe
	{
		Mode mode = Mode::no_devices;
		int interceptions = 0;
		HRESULT warp_failure = S_OK;
		int warp_interceptions = 0;
		int hardware_adapters = 0;
		int hardware_descriptions = 0;
	};

	inline thread_local Probe* active_probe = nullptr;

	class Scope
	{
	public:
		explicit Scope(Probe& probe)
		{
			if (active_probe != nullptr)
			{
				throw std::logic_error("DXGI factory probes cannot nest");
			}
			active_probe = &probe;
		}

		~Scope() { active_probe = nullptr; }
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	};

	// Description controls are inputs to selection or forced-failure calls.
	// Modified metadata must never provide successful hardware-device evidence.
	class DescriptionControl final : public IDXGIAdapter1
	{
	public:
		DescriptionControl(Microsoft::WRL::ComPtr<IDXGIAdapter1> real, Mode mode)
			: real_(std::move(real)), mode_(mode)
		{
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
		{
			if (object == nullptr)
				return E_POINTER;
			*object = nullptr;
			if (iid == __uuidof(IUnknown) || iid == __uuidof(IDXGIObject) ||
				iid == __uuidof(IDXGIAdapter) || iid == __uuidof(IDXGIAdapter1))
			{
				*object = static_cast<IDXGIAdapter1*>(this);
				this->AddRef();
				return S_OK;
			}
			return this->real_->QueryInterface(iid, object);
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return this->references_.fetch_add(1) + 1;
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG remaining = this->references_.fetch_sub(1) - 1;
			if (remaining == 0)
				delete this;
			return remaining;
		}

		HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID name, UINT size,
			const void* data) override
		{
			return this->real_->SetPrivateData(name, size, data);
		}

		HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID name,
			const IUnknown* object) override
		{
			return this->real_->SetPrivateDataInterface(name, object);
		}

		HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID name, UINT* size,
			void* data) override
		{
			return this->real_->GetPrivateData(name, size, data);
		}

		HRESULT STDMETHODCALLTYPE GetParent(REFIID iid, void** parent) override
		{
			return this->real_->GetParent(iid, parent);
		}

		HRESULT STDMETHODCALLTYPE EnumOutputs(UINT index, IDXGIOutput** output) override
		{
			return this->real_->EnumOutputs(index, output);
		}

		HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC* description) override
		{
			const HRESULT result = this->real_->GetDesc(description);
			if (SUCCEEDED(result))
			{
				this->describe(*description);
			}
			return result;
		}

		HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(REFGUID name,
			LARGE_INTEGER* version) override
		{
			return this->real_->CheckInterfaceSupport(name, version);
		}

		HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1* description) override
		{
			const HRESULT result = this->real_->GetDesc1(description);
			if (SUCCEEDED(result))
			{
				this->describe(*description);
				description->Flags = 0;
			}
			return result;
		}

	private:
		template<class Description>
		void describe(Description& description) const
		{
			if (this->mode_ == Mode::unflagged_software_ids)
			{
				wcscpy_s(description.Description, L"Labrador software ID control");
				description.VendorId = 0x1414;
				description.DeviceId = 0x8c;
			}
			else
			{
				wcscpy_s(description.Description, this->mode_ == Mode::named_software
					? L"mIcRoSoFt BaSiC ReNdEr DrIvEr" : L"Labrador synthetic hardware control");
				description.VendorId = 0x1234;
				description.DeviceId = 0x5678;
			}
			if (active_probe != nullptr && this->mode_ == Mode::hardware_control)
				++active_probe->hardware_descriptions;
		}

		~DescriptionControl() = default;
		std::atomic<ULONG> references_{ 1 };
		Microsoft::WRL::ComPtr<IDXGIAdapter1> real_;
		Mode mode_;
	};

	class Factory final : public IDXGIFactory6
	{
	public:
		explicit Factory(Microsoft::WRL::ComPtr<IDXGIFactory6> real)
			: real_(std::move(real))
		{
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
		{
			if (object == nullptr)
				return E_POINTER;
			*object = nullptr;
			if (iid == __uuidof(IUnknown) || iid == __uuidof(IDXGIObject) ||
				iid == __uuidof(IDXGIFactory) || iid == __uuidof(IDXGIFactory1) ||
				iid == __uuidof(IDXGIFactory2) || iid == __uuidof(IDXGIFactory3) ||
				iid == __uuidof(IDXGIFactory4) || iid == __uuidof(IDXGIFactory5) ||
				iid == __uuidof(IDXGIFactory6))
			{
				*object = static_cast<IDXGIFactory6*>(this);
				this->AddRef();
				return S_OK;
			}
			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return this->references_.fetch_add(1) + 1;
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG remaining = this->references_.fetch_sub(1) - 1;
			if (remaining == 0)
				delete this;
			return remaining;
		}

		HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID name, UINT size,
			const void* data) override
		{
			return this->real_->SetPrivateData(name, size, data);
		}

		HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID name,
			const IUnknown* object) override
		{
			return this->real_->SetPrivateDataInterface(name, object);
		}

		HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID name, UINT* size,
			void* data) override
		{
			return this->real_->GetPrivateData(name, size, data);
		}

		HRESULT STDMETHODCALLTYPE GetParent(REFIID iid, void** parent) override
		{
			return this->real_->GetParent(iid, parent);
		}

		HRESULT STDMETHODCALLTYPE EnumAdapters(UINT index, IDXGIAdapter** adapter) override
		{
			return this->real_->EnumAdapters(index, adapter);
		}

		HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND window, UINT flags) override
		{
			return this->real_->MakeWindowAssociation(window, flags);
		}

		HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* window) override
		{
			return this->real_->GetWindowAssociation(window);
		}

		HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* device,
			DXGI_SWAP_CHAIN_DESC* description, IDXGISwapChain** chain) override
		{
			return this->real_->CreateSwapChain(device, description, chain);
		}

		HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE module,
			IDXGIAdapter** adapter) override
		{
			return this->real_->CreateSoftwareAdapter(module, adapter);
		}

		HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT index, IDXGIAdapter1** adapter) override
		{
			if (active_probe != nullptr)
				return this->injected_adapter(index, __uuidof(IDXGIAdapter1),
					reinterpret_cast<void**>(adapter));
			return this->real_->EnumAdapters1(index, adapter);
		}

		BOOL STDMETHODCALLTYPE IsCurrent() override
		{
			return this->real_->IsCurrent();
		}

		BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled() override
		{
			return this->real_->IsWindowedStereoEnabled();
		}

		HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown* device, HWND window,
			const DXGI_SWAP_CHAIN_DESC1* description,
			const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full_screen,
			IDXGIOutput* output, IDXGISwapChain1** chain) override
		{
			return this->real_->CreateSwapChainForHwnd(device, window, description,
				full_screen, output, chain);
		}

		HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(IUnknown* device,
			IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* description,
			IDXGIOutput* output, IDXGISwapChain1** chain) override
		{
			return this->real_->CreateSwapChainForCoreWindow(device, window,
				description, output, chain);
		}

		HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(HANDLE resource,
			LUID* luid) override
		{
			return this->real_->GetSharedResourceAdapterLuid(resource, luid);
		}

		HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(HWND window,
			UINT message, DWORD* cookie) override
		{
			return this->real_->RegisterStereoStatusWindow(window, message, cookie);
		}

		HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(HANDLE event,
			DWORD* cookie) override
		{
			return this->real_->RegisterStereoStatusEvent(event, cookie);
		}

		void STDMETHODCALLTYPE UnregisterStereoStatus(DWORD cookie) override
		{
			this->real_->UnregisterStereoStatus(cookie);
		}

		HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(HWND window,
			UINT message, DWORD* cookie) override
		{
			return this->real_->RegisterOcclusionStatusWindow(window, message, cookie);
		}

		HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(HANDLE event,
			DWORD* cookie) override
		{
			return this->real_->RegisterOcclusionStatusEvent(event, cookie);
		}

		void STDMETHODCALLTYPE UnregisterOcclusionStatus(DWORD cookie) override
		{
			this->real_->UnregisterOcclusionStatus(cookie);
		}

		HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(IUnknown* device,
			const DXGI_SWAP_CHAIN_DESC1* description, IDXGIOutput* output,
			IDXGISwapChain1** chain) override
		{
			return this->real_->CreateSwapChainForComposition(device, description,
				output, chain);
		}

		UINT STDMETHODCALLTYPE GetCreationFlags() override
		{
			return this->real_->GetCreationFlags();
		}

		HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(LUID luid, REFIID iid,
			void** adapter) override
		{
			return this->real_->EnumAdapterByLuid(luid, iid, adapter);
		}

		HRESULT STDMETHODCALLTYPE EnumWarpAdapter(REFIID iid, void** adapter) override
		{
			if (active_probe != nullptr && FAILED(active_probe->warp_failure))
			{
				active_probe->warp_interceptions++;
				*adapter = nullptr;
				return active_probe->warp_failure;
			}
			return this->real_->EnumWarpAdapter(iid, adapter);
		}

		HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE feature,
			void* data, UINT size) override
		{
			return this->real_->CheckFeatureSupport(feature, data, size);
		}

		HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT index,
			DXGI_GPU_PREFERENCE preference, REFIID iid, void** adapter) override
		{
			if (active_probe != nullptr)
				return this->injected_adapter(index, iid, adapter);
			return this->real_->EnumAdapterByGpuPreference(index, preference, iid, adapter);
		}

	private:
		~Factory() = default;

		HRESULT injected_adapter(UINT index, REFIID iid, void** adapter)
		{
			if (adapter == nullptr)
				return E_POINTER;
			*adapter = nullptr;
			++active_probe->interceptions;
			if (active_probe->mode == Mode::error)
				return E_OUTOFMEMORY;
			if (active_probe->mode == Mode::software_only && index == 0)
				return this->real_->EnumWarpAdapter(iid, adapter);
			const Mode mode = active_probe->mode;
			const bool mixed = mode == Mode::software_then_hardware;
			if ((index == 0 && (mode == Mode::hardware_control ||
				mode == Mode::unflagged_software_ids || mode == Mode::named_software || mixed)) ||
				(mixed && index == 1))
			{
				const Mode description_mode = mixed
					? (index == 0 ? Mode::named_software : Mode::hardware_control) : mode;
				Microsoft::WRL::ComPtr<IDXGIAdapter1> warp;
				const HRESULT found = this->real_->EnumWarpAdapter(
					IID_PPV_ARGS(warp.GetAddressOf()));
				if (FAILED(found))
					return found;
				DescriptionControl* control = new(std::nothrow) DescriptionControl(std::move(warp), description_mode);
				if (control == nullptr)
					return E_OUTOFMEMORY;
				const HRESULT queried = control->QueryInterface(iid, adapter);
				control->Release();
				if (SUCCEEDED(queried) && description_mode == Mode::hardware_control)
					++active_probe->hardware_adapters;
				return queried;
			}
			return DXGI_ERROR_NOT_FOUND;
		}

		std::atomic<ULONG> references_{ 1 };
		Microsoft::WRL::ComPtr<IDXGIFactory6> real_;
	};

	// Call after a successful real CreateDXGIFactory1/2. The incoming COM
	// reference is released only after the replacement has been queried in the
	// requested interface; failure leaves the original pointer owned by caller.
	inline HRESULT wrap_factory(REFIID iid, void** factory)
	{
		if (active_probe == nullptr)
			return S_OK;
		if (factory == nullptr || *factory == nullptr)
			return E_POINTER;
		IUnknown* original = static_cast<IUnknown*>(*factory);
		Microsoft::WRL::ComPtr<IDXGIFactory6> real;
		const HRESULT available = original->QueryInterface(IID_PPV_ARGS(real.GetAddressOf()));
		if (FAILED(available))
			return available;
		Factory* proxy = new(std::nothrow) Factory(std::move(real));
		if (proxy == nullptr)
			return E_OUTOFMEMORY;
		void* replacement = nullptr;
		const HRESULT wrapped = proxy->QueryInterface(iid, &replacement);
		proxy->Release();
		if (FAILED(wrapped))
			return wrapped;
		original->Release();
		*factory = replacement;
		return S_OK;
	}
}

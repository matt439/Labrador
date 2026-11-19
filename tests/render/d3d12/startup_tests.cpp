#include <doctest/doctest.h>

#include "engine/render/d3d12/device_resources.h"
#include "engine/render/device_unavailable.h"
#include "engine/render/renderer.h"
#include "engine/render/throw_if_failed.h"
#include "tests/render/direct3d_startup_fixture.h"
#include "tests/render/test_window.h"
#include "tests/render/win32/import_hook.h"

#include <string>

extern "C"
{
	extern decltype(&D3D12CreateDevice) __imp_D3D12CreateDevice;
	extern decltype(&CreateDXGIFactory2) __imp_CreateDXGIFactory2;
}

namespace
{
	HRESULT WINAPI create_device(IUnknown* adapter, D3D_FEATURE_LEVEL level,
		REFIID iid, void** device)
	{
		using namespace render_tests::direct3d_startup;
		if (active != nullptr)
		{
			HRESULT failure;
			if (device == nullptr)
			{
				active->capability_attempts++;
				failure = active->capability_failure;
			}
			else
			{
				active->creation_attempts++;
				failure = active->creation_attempts > 1 && FAILED(active->fallback_failure)
					? active->fallback_failure : active->creation_failure;
			}
			if (FAILED(failure))
			{
				if (device != nullptr) { *device = nullptr; }
				return failure;
			}
			if (device == nullptr && active->capability_success)
			{
				return S_FALSE;
			}
		}
		static const decltype(&D3D12CreateDevice) real =
			system_function<decltype(&D3D12CreateDevice)>(L"d3d12.dll", "D3D12CreateDevice");
		return real(adapter, level, iid, device);
	}

	void create_resources(bool startup)
	{
		labrador::DeviceResources resources;
		resources.create_device_resources(startup);
	}

	struct ImportScope
	{
		render_tests::ImportHook<decltype(&D3D12CreateDevice)> device{
			__imp_D3D12CreateDevice, create_device };
		render_tests::ImportHook<decltype(&CreateDXGIFactory2)> factory2{
			__imp_CreateDXGIFactory2, render_tests::direct3d_startup::create_factory2 };
	};
}

#if !defined(LABRADOR_D3D_SOFTWARE_TEST)
TEST_CASE("D3D STARTUP: a failed D3D12 capability query is not unsupported hardware")
{
	ImportScope imports;
	using namespace render_tests::direct3d_startup;
	render_tests::dxgi_factory::Probe adapter{ render_tests::dxgi_factory::Mode::hardware_control };
	render_tests::dxgi_factory::Scope enumeration(adapter);
	Probe probe;
	probe.capability_failure = E_OUTOFMEMORY;
	Scope scope(probe);
	try
	{
		create_resources(true);
		FAIL("the capability API error was ignored");
	}
	catch (const labrador::RenderDeviceUnavailable&)
	{
		FAIL("an allocation error is not a missing render device");
	}
	catch (const std::runtime_error& error)
	{
		CHECK(std::string(error.what()).find("8007000E") != std::string::npos);
	}
	CHECK(probe.capability_attempts > 0);
	CHECK(adapter.hardware_adapters > 0);
	CHECK(adapter.hardware_descriptions > 0);
}
#endif

#if defined(_DEBUG) || defined(LABRADOR_D3D_SOFTWARE_TEST)
TEST_CASE("D3D STARTUP: an explicit WARP lookup distinguishes absence from API failure")
{
	ImportScope imports;
	using namespace render_tests::dxgi_factory;
	for (const HRESULT result : { DXGI_ERROR_NOT_FOUND, DXGI_ERROR_UNSUPPORTED, E_OUTOFMEMORY })
	{
		Probe probe;
		probe.warp_failure = result;
		Scope scope(probe);
		bool unavailable = false;
		bool api_error = false;
		try
		{
			create_resources(true);
			FAIL("the forced WARP lookup failure was ignored");
		}
		catch (const labrador::RenderDeviceUnavailable& error)
		{
			unavailable = true;
			CHECK(std::string(error.what()).find(labrador::hresult_name(result)) != std::string::npos);
		}
		catch (const std::runtime_error& error)
		{
			api_error = true;
			CHECK(std::string(error.what()).find(labrador::hresult_name(result)) != std::string::npos);
		}
		CHECK(unavailable == (result != E_OUTOFMEMORY));
		CHECK(api_error == (result == E_OUTOFMEMORY));
		CHECK(probe.warp_interceptions > 0);
	}
}
#endif

#include "tests/render/direct3d_startup_cases.h"

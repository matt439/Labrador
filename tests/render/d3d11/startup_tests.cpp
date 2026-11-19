#include <doctest/doctest.h>

#include "engine/render/d3d11/device_resources.h"
#include "engine/render/device_unavailable.h"
#include "engine/render/renderer.h"
#include "tests/render/direct3d_startup_fixture.h"
#include "tests/render/test_window.h"
#include "tests/render/win32/import_hook.h"

extern "C"
{
	extern decltype(&D3D11CreateDevice) __imp_D3D11CreateDevice;
	extern decltype(&CreateDXGIFactory1) __imp_CreateDXGIFactory1;
	extern decltype(&CreateDXGIFactory2) __imp_CreateDXGIFactory2;
}

namespace
{
	HRESULT WINAPI create_device(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver,
		HMODULE software, UINT flags, const D3D_FEATURE_LEVEL* levels, UINT count,
		UINT version, ID3D11Device** device, D3D_FEATURE_LEVEL* selected,
		ID3D11DeviceContext** context)
	{
		using namespace render_tests::direct3d_startup;
		if (active != nullptr && driver != D3D_DRIVER_TYPE_NULL && device != nullptr)
		{
			active->creation_attempts++;
			const HRESULT failure = driver == D3D_DRIVER_TYPE_WARP && FAILED(active->fallback_failure)
				? active->fallback_failure : active->creation_failure;
			if (FAILED(failure))
			{
				*device = nullptr;
				if (context != nullptr) { *context = nullptr; }
				return failure;
			}
		}
		static const decltype(&D3D11CreateDevice) real =
			system_function<decltype(&D3D11CreateDevice)>(L"d3d11.dll", "D3D11CreateDevice");
		return real(adapter, driver, software, flags, levels, count, version,
			device, selected, context);
	}

	void create_resources(bool startup)
	{
		labrador::DeviceResources resources(DXGI_FORMAT_B8G8R8A8_UNORM);
		resources.CreateDeviceResources(startup);
	}

	struct ImportScope
	{
		render_tests::ImportHook<decltype(&D3D11CreateDevice)> device{
			__imp_D3D11CreateDevice, create_device };
		render_tests::ImportHook<decltype(&CreateDXGIFactory1)> factory1{
			__imp_CreateDXGIFactory1, render_tests::direct3d_startup::create_factory1 };
		render_tests::ImportHook<decltype(&CreateDXGIFactory2)> factory2{
			__imp_CreateDXGIFactory2, render_tests::direct3d_startup::create_factory2 };
	};
}

#include "tests/render/direct3d_startup_cases.h"

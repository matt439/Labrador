#pragma once

#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"

#include <Windows.h>
#include <stdexcept>

namespace render_tests
{
	class HiddenWindow
	{
	public:
		HiddenWindow()
		{
			handle = CreateWindowExW(0, L"STATIC", L"texture refusal test",
				WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
			if (handle == nullptr) throw std::runtime_error("hidden texture test window failed");
		}
		~HiddenWindow() { DestroyWindow(handle); }
		HiddenWindow(const HiddenWindow&) = delete;
		HiddenWindow& operator=(const HiddenWindow&) = delete;
		HWND handle = nullptr;
	};

	struct DeviceFixture
	{
		HiddenWindow window;
		labrador::RenderResources resources;
		labrador::Renderer renderer;
		DeviceFixture()
		{
			renderer.create_device(window.handle, 64, 64, 1);
			renderer.set_resources(&resources);
		}
		~DeviceFixture() { resources.release_device_resources(); }
	};
}

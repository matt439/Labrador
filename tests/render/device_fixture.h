#pragma once

#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"

#include "tests/render/test_window.h"

namespace render_tests
{
	struct DeviceFixture
	{
		// First, so it is the last to go: the renderer's device was made on it.
		TestWindow window{ 64, 64 };
		labrador::RenderResources resources;
		labrador::Renderer renderer;
		DeviceFixture()
		{
			renderer.create_device(window.handle(), 64, 64, 1);
			renderer.set_resources(&resources);
		}
		~DeviceFixture() { resources.release_device_resources(); }
	};
}

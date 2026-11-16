#include <doctest/doctest.h>

#include "engine/render/device_unavailable.h"

#include <stdexcept>
#include <string>
#include <type_traits>

using namespace labrador;

TEST_SUITE("RenderDeviceUnavailableTests")
{
	TEST_CASE("a consumer catches device absence without parsing its reason")
	{
		static_assert(std::is_base_of_v<std::runtime_error, RenderDeviceUnavailable>);
		std::string reason = "Vulkan: no physical device supports timeline semaphores";
		bool device_unavailable = false;
		try
		{
			throw RenderDeviceUnavailable(reason);
		}
		catch (const RenderDeviceUnavailable& error)
		{
			device_unavailable = true;
			reason.clear();
			CHECK(std::string(error.what()) ==
				"Vulkan: no physical device supports timeline semaphores");
		}
		catch (const std::runtime_error&)
		{
			FAIL("the narrower consumer handler must run first");
		}
		CHECK(device_unavailable);
	}

	TEST_CASE("existing runtime_error consumers keep the complete native reason")
	{
		bool caught = false;
		try
		{
			throw RenderDeviceUnavailable(
				"Direct3D 12 requires feature level 11.0 (HRESULT 887A0004).");
		}
		catch (const std::runtime_error& error)
		{
			caught = true;
			CHECK(std::string(error.what()) ==
				"Direct3D 12 requires feature level 11.0 (HRESULT 887A0004).");
		}
		CHECK(caught);
	}

	TEST_CASE("an unrelated runtime error does not enter the optional graphics handler")
	{
		bool ordinary_error = false;
		try
		{
			throw std::runtime_error("authored shader compilation failed");
		}
		catch (const RenderDeviceUnavailable&)
		{
			FAIL("a broad runtime error is not a device-absence category");
		}
		catch (const std::runtime_error& error)
		{
			ordinary_error = true;
			CHECK(std::string(error.what()) == "authored shader compilation failed");
		}
		CHECK(ordinary_error);
	}
}

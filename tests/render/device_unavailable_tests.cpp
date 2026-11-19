#include <doctest/doctest.h>

#include "engine/render/device_unavailable.h"

#ifdef _WIN32
#include "engine/render/direct3d_startup.h"
#endif

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

#ifdef _WIN32
TEST_CASE("Direct3D startup classification retains all unrelated native failures")
{
	const char* reason = "The requested Direct3D device could not be created";
	CHECK_NOTHROW(check_direct3d_creation(S_OK, true, reason));
	try
	{
		check_direct3d_creation(DXGI_ERROR_UNSUPPORTED, true, reason);
		FAIL("unsupported startup must use the narrower category");
	}
	catch (const RenderDeviceUnavailable& error)
	{
		CHECK(std::string(error.what()) ==
			"The requested Direct3D device could not be created (HRESULT 887A0004).");
	}
	for (const HRESULT result : { E_OUTOFMEMORY, E_INVALIDARG, E_FAIL,
		DXGI_ERROR_DEVICE_REMOVED, DXGI_ERROR_SDK_COMPONENT_MISSING })
	{
		try
		{
			check_direct3d_creation(result, true, reason);
			FAIL("a failed native call was ignored");
		}
		catch (const RenderDeviceUnavailable&)
		{
			FAIL("an unrelated native failure is not startup capability absence");
		}
		catch (const com_exception& error)
		{
			CHECK(std::string(error.what()).find(hresult_name(result)) != std::string::npos);
		}
	}
	CHECK_THROWS_AS(check_direct3d_creation(DXGI_ERROR_UNSUPPORTED, false, reason), com_exception);
}

TEST_CASE("an explicit WARP lookup distinguishes adapter absence from API errors")
{
	CHECK_NOTHROW(check_direct3d_warp_adapter(S_OK, true, "WARP lookup"));
	for (const HRESULT result : { DXGI_ERROR_NOT_FOUND, DXGI_ERROR_UNSUPPORTED })
	{
		try
		{
			check_direct3d_warp_adapter(result, true, "WARP lookup");
			FAIL("the requested WARP adapter is unavailable");
		}
		catch (const RenderDeviceUnavailable& error)
		{
			CHECK(std::string(error.what()).find(hresult_name(result)) != std::string::npos);
		}
		CHECK_THROWS_AS(check_direct3d_warp_adapter(result, false, "WARP lookup"), com_exception);
	}
	for (const HRESULT result : { E_OUTOFMEMORY, E_INVALIDARG, E_FAIL })
	{
		CHECK_THROWS_AS(check_direct3d_warp_adapter(result, true, "WARP lookup"), com_exception);
	}
}
#endif

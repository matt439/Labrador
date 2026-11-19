#pragma once

#include <initializer_list>
#include <stdexcept>
#include <string>

TEST_SUITE("Direct3DStartupTests")
{
	TEST_CASE("D3D STARTUP: invalid caller window fails before adapter selection")
	{
		ImportScope imports;
		render_tests::direct3d_startup::Probe device;
		device.creation_failure = DXGI_ERROR_UNSUPPORTED;
		render_tests::direct3d_startup::Scope creation(device);
		labrador::Renderer renderer;
		CHECK_THROWS_AS(renderer.create_device(nullptr, 32, 32, 1), std::logic_error);
		CHECK(device.creation_attempts == 0);
	}

#if defined(_DEBUG) && !defined(LABRADOR_D3D_SOFTWARE_TEST)
	TEST_CASE("D3D STARTUP: debug fallback cannot relabel an earlier unrelated failure")
	{
		ImportScope imports;
		using namespace render_tests::direct3d_startup;
		render_tests::dxgi_factory::Probe adapter{ render_tests::dxgi_factory::Mode::hardware_control };
		render_tests::dxgi_factory::Scope enumeration(adapter);
		Probe probe;
		probe.creation_failure = E_OUTOFMEMORY;
		probe.fallback_failure = DXGI_ERROR_UNSUPPORTED;
		probe.capability_success = true;
		Scope scope(probe);
		try
		{
			create_resources(true);
			FAIL("both forced device failures must escape startup");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("fallback failure must not reclassify the first unrelated error");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("887A0004") != std::string::npos);
		}
		CHECK(probe.creation_attempts == 2);
	}
#endif

	TEST_CASE("D3D STARTUP: unsupported device creation has a typed native diagnostic")
	{
		ImportScope imports;
		using namespace render_tests::direct3d_startup;
		render_tests::dxgi_factory::Probe adapter{ render_tests::dxgi_factory::Mode::hardware_control };
		render_tests::dxgi_factory::Scope enumeration(adapter);
		Probe probe;
		probe.creation_failure = DXGI_ERROR_UNSUPPORTED;
		probe.capability_success = true;
		Scope scope(probe);
		render_tests::TestWindow window(32, 32);
		labrador::Renderer renderer;
		try
		{
			renderer.create_device(window.handle(), 32, 32, 1);
			FAIL("the selected renderer must refuse unsupported device creation");
		}
		catch (const labrador::RenderDeviceUnavailable& error)
		{
			CHECK(std::string(error.what()).find("887A0004") != std::string::npos);
		}
		CHECK(probe.creation_attempts > 0);
#if !defined(LABRADOR_D3D_SOFTWARE_TEST)
		CHECK(adapter.hardware_adapters > 0);
		CHECK(adapter.hardware_descriptions > 0);
#endif
	}

#if !defined(LABRADOR_D3D_SOFTWARE_TEST)
#if defined(NDEBUG)
	TEST_CASE("D3D STARTUP: default Release refuses known software without device creation")
	{
		for (const render_tests::dxgi_factory::Mode mode : {
			render_tests::dxgi_factory::Mode::software_only,
			render_tests::dxgi_factory::Mode::unflagged_software_ids,
			render_tests::dxgi_factory::Mode::named_software })
		{
			ImportScope imports;
			render_tests::dxgi_factory::Probe adapters{ mode };
			render_tests::dxgi_factory::Scope enumeration(adapters);
			render_tests::direct3d_startup::Probe device;
			device.creation_failure = E_INVALIDARG;
			device.capability_success = true;
			render_tests::direct3d_startup::Scope creation(device);
			CHECK_THROWS_AS(create_resources(true), labrador::RenderDeviceUnavailable);
			CHECK(adapters.interceptions > 0);
			CHECK(device.creation_attempts == 0);
			CHECK(device.capability_attempts == 0);
		}
	}
#endif

	TEST_CASE("D3D STARTUP: unflagged software IDs and names are refused before device creation")
	{
		for (const render_tests::dxgi_factory::Mode mode : {
			render_tests::dxgi_factory::Mode::unflagged_software_ids,
			render_tests::dxgi_factory::Mode::named_software })
		{
			ImportScope imports;
			render_tests::dxgi_factory::Probe adapters{ mode };
			render_tests::dxgi_factory::Scope enumeration(adapters);
			render_tests::direct3d_startup::Probe device;
			device.creation_failure = DXGI_ERROR_UNSUPPORTED;
			device.capability_success = true;
			render_tests::direct3d_startup::Scope creation(device);
			CHECK_THROWS_AS(create_resources(true), labrador::RenderDeviceUnavailable);
			CHECK(adapters.interceptions > 0);
			CHECK(device.capability_attempts == 0);
#if defined(NDEBUG)
			CHECK(device.creation_attempts == 0);
#else
			// Ordinary Debug still attempts the separate WARP fallback.
			CHECK(device.creation_attempts == 1);
#endif
		}
	}

	TEST_CASE("D3D STARTUP: selection skips known software and reaches the next hardware control")
	{
		ImportScope imports;
		render_tests::dxgi_factory::Probe adapters{
			render_tests::dxgi_factory::Mode::software_then_hardware };
		render_tests::dxgi_factory::Scope enumeration(adapters);
		render_tests::direct3d_startup::Probe device;
		device.creation_failure = E_OUTOFMEMORY;
		device.capability_success = true;
		render_tests::direct3d_startup::Scope creation(device);
		try
		{
			create_resources(true);
			FAIL("the selected hardware control's forced allocation failure did not escape");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("the hardware control was lost after the software candidate");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("8007000E") != std::string::npos);
		}
		CHECK(adapters.hardware_adapters > 0);
		CHECK(adapters.hardware_descriptions > 0);
		CHECK(device.creation_attempts > 0);
	}

	TEST_CASE("D3D STARTUP: exhausted device enumeration differs from an API failure")
	{
		ImportScope imports;
		using namespace render_tests;
		dxgi_factory::Probe adapters{ dxgi_factory::Mode::no_devices };
		dxgi_factory::Scope enumeration(adapters);
		direct3d_startup::Probe device;
		device.creation_failure = DXGI_ERROR_UNSUPPORTED;
		direct3d_startup::Scope creation(device);
		CHECK_THROWS_AS(create_resources(true), labrador::RenderDeviceUnavailable);
		CHECK(adapters.interceptions > 0);
	}

	TEST_CASE("D3D STARTUP: software-only enumeration uses the real selection predicate")
	{
		ImportScope imports;
		using namespace render_tests;
		dxgi_factory::Probe adapters{ dxgi_factory::Mode::software_only };
		dxgi_factory::Scope enumeration(adapters);
		direct3d_startup::Probe device;
		device.creation_failure = DXGI_ERROR_UNSUPPORTED;
		direct3d_startup::Scope creation(device);
		CHECK_THROWS_AS(create_resources(true), labrador::RenderDeviceUnavailable);
		CHECK(adapters.interceptions > 0);
	}

	TEST_CASE("D3D STARTUP: enumeration errors retain their native API category")
	{
		ImportScope imports;
		using namespace render_tests;
		dxgi_factory::Probe adapters{ dxgi_factory::Mode::error };
		dxgi_factory::Scope scope(adapters);
		try
		{
			create_resources(true);
			FAIL("the failed adapter enumeration was ignored");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("an enumeration allocation failure is not device absence");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("8007000E") != std::string::npos);
		}
		CHECK(adapters.interceptions > 0);
	}
#else
	TEST_CASE("D3D STARTUP: an explicit software-test build creates the real WARP device")
	{
		render_tests::TestWindow window(32, 32);
		labrador::Renderer renderer;
		renderer.create_device(window.handle(), 32, 32, 1);
		CHECK(renderer.device_info().kind == labrador::RenderDeviceKind::software);
	}
#endif

	TEST_CASE("D3D STARTUP: device allocation and argument errors remain ordinary failures")
	{
		ImportScope imports;
		using namespace render_tests::direct3d_startup;
		render_tests::dxgi_factory::Probe adapter{ render_tests::dxgi_factory::Mode::hardware_control };
		render_tests::dxgi_factory::Scope enumeration(adapter);
		for (const HRESULT failure : { E_OUTOFMEMORY, E_INVALIDARG })
		{
			Probe probe;
			probe.creation_failure = failure;
			probe.capability_success = true;
			Scope scope(probe);
			try
			{
				create_resources(true);
				FAIL("the device API failure was ignored");
			}
			catch (const labrador::RenderDeviceUnavailable&)
			{
				FAIL("an allocation or programming failure is not device absence");
			}
			catch (const std::runtime_error&) {}
			CHECK(probe.creation_attempts > 0);
#if !defined(LABRADOR_D3D_SOFTWARE_TEST)
			CHECK(adapter.hardware_adapters > 0);
			CHECK(adapter.hardware_descriptions > 0);
#endif
		}
	}

	TEST_CASE("D3D RECOVERY: unsupported recreation retains runtime_error compatibility")
	{
		ImportScope imports;
		using namespace render_tests::direct3d_startup;
		render_tests::dxgi_factory::Probe adapter{ render_tests::dxgi_factory::Mode::hardware_control };
		render_tests::dxgi_factory::Scope enumeration(adapter);
		Probe probe;
		probe.creation_failure = DXGI_ERROR_UNSUPPORTED;
		probe.capability_success = true;
		Scope scope(probe);
		try
		{
			// The same method recovery calls, with its default false startup flag.
			create_resources(false);
			FAIL("unsupported recreation unexpectedly succeeded");
		}
		catch (const labrador::RenderDeviceUnavailable&)
		{
			FAIL("runtime recovery must not enter a startup refusal handler");
		}
		catch (const std::runtime_error& error)
		{
			CHECK(std::string(error.what()).find("887A0004") != std::string::npos);
		}
#if !defined(LABRADOR_D3D_SOFTWARE_TEST)
		CHECK(adapter.hardware_adapters > 0);
		CHECK(adapter.hardware_descriptions > 0);
#endif
	}
}

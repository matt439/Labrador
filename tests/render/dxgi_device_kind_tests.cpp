#include <doctest/doctest.h>

#include "engine/render/dxgi_device_kind.h"

using namespace labrador;

// How both Direct3D backends classify the adapter they drew on, from the
// values a DXGI_ADAPTER_DESC1 carries. No device is made: the function takes
// plain integers and a name, so this runs in every configuration.
//
// The second case is GitHub issue #125's report: a machine with no GPU whose
// adapter is the Microsoft Basic Render Driver without
// DXGI_ADAPTER_FLAG_SOFTWARE, which the flag alone classified as hardware.

TEST_CASE("an adapter carrying DXGI's software flag is software")
{
	CHECK(dxgi_device_kind(dxgi_adapter_flag_software, 0x10de, 0x2c02,
		"NVIDIA GeForce RTX 5080") == RenderDeviceKind::software);
	CHECK(dxgi_device_kind(dxgi_adapter_flag_software | 1, 0, 0, "")
		== RenderDeviceKind::software);
}

TEST_CASE("the Basic Render Driver is software without the flag")
{
	// The triple issue #125 reports.
	CHECK(dxgi_device_kind(0, 0x1414, 0x8c, "Microsoft Basic Render Driver")
		== RenderDeviceKind::software);

	// Either signal alone is enough.
	CHECK(dxgi_device_kind(0, 0x1414, 0x8c, "") == RenderDeviceKind::software);
	CHECK(dxgi_device_kind(0, 0, 0, "MICROSOFT BASIC RENDER DRIVER")
		== RenderDeviceKind::software);
}

TEST_CASE("an ordinary adapter is hardware")
{
	CHECK(dxgi_device_kind(0, 0x10de, 0x2c02, "NVIDIA GeForce RTX 5080")
		== RenderDeviceKind::hardware);

	// Microsoft's vendor number alone is not the Basic Render Driver, and
	// neither is its device number under another vendor.
	CHECK(dxgi_device_kind(0, 0x1414, 0x8d, "Another Microsoft adapter")
		== RenderDeviceKind::hardware);
	CHECK(dxgi_device_kind(0, 0x8086, 0x8c, "Intel adapter")
		== RenderDeviceKind::hardware);

	// The remote-desktop flag (DXGI_ADAPTER_FLAG_REMOTE) is not the
	// software flag.
	CHECK(dxgi_device_kind(1, 0x1002, 0x744c, "AMD Radeon")
		== RenderDeviceKind::hardware);
}

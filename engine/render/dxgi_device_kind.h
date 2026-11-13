#pragma once

#include "engine/render/renderer.h"

#include <cstdint>
#include <string_view>

// The Direct3D backends' answer to RenderDeviceInfo::kind, made once for
// both and from plain values, so that every configuration can test it.
//
// It lives beside throw_if_failed.h for the reason that file gives: its only
// engine callers are the two Direct3D backends, and one shared file cannot
// disagree with itself. It includes no Windows header, so it also compiles in
// the browser and null configurations, where RenderTests runs it.

namespace labrador
{
	// DXGI_ADAPTER_FLAG_SOFTWARE's value. Each Direct3D backend checks it
	// against the SDK's own constant at compile time.
	inline constexpr std::uint32_t dxgi_adapter_flag_software = 2;

	// The Microsoft Basic Render Driver's PCI vendor and device numbers, the
	// adapter DXGI offers WARP through.
	inline constexpr std::uint32_t microsoft_basic_render_vendor_id = 0x1414;
	inline constexpr std::uint32_t microsoft_basic_render_device_id = 0x8c;

	// The kind of a DXGI adapter, from the fields of its DXGI_ADAPTER_DESC1
	// (`device_name` is its Description, narrowed to UTF-8).
	//
	// `software` when the flags carry DXGI_ADAPTER_FLAG_SOFTWARE, when the
	// vendor and device numbers are the Basic Render Driver's, or when the
	// name contains "Microsoft Basic Render" in any case: an adapter can be a
	// CPU rasteriser without carrying the flag. `hardware` otherwise, which
	// means only that none of those three said software.
	RenderDeviceKind dxgi_device_kind(std::uint32_t flags,
		std::uint32_t vendor_id, std::uint32_t device_id,
		std::string_view device_name);
}

#pragma once

#include "engine/render/device_unavailable.h"
#include "engine/render/throw_if_failed.h"

namespace labrador
{
	// Used only for Direct3D device creation (or an explicit WARP request).
	// UNSUPPORTED states a capability refusal. Allocation, invalid arguments,
	// lost devices and other native failures retain their COM error category.
	// Recovery passes false even when the same creation API is used again.
	inline void check_direct3d_creation(HRESULT result, bool startup, const char* reason)
	{
		if (startup && result == DXGI_ERROR_UNSUPPORTED)
		{
			throw RenderDeviceUnavailable(std::string(reason) + " (" +
				hresult_name(result) + ").");
		}
		ThrowIfFailed(result, reason);
	}

	// An explicit WARP request has exactly one adapter to find. NOT_FOUND is
	// therefore absence here, whereas an ordinary enumeration loop uses it as
	// successful exhaustion before deciding whether any candidate was usable.
	inline void check_direct3d_warp_adapter(HRESULT result, bool startup, const char* reason)
	{
		if (startup && (result == DXGI_ERROR_NOT_FOUND || result == DXGI_ERROR_UNSUPPORTED))
		{
			throw RenderDeviceUnavailable(std::string(reason) + " (" +
				hresult_name(result) + ").");
		}
		ThrowIfFailed(result, reason);
	}
}

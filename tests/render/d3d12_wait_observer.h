#pragma once

#include <Windows.h>

namespace labrador::d3d12_wait_test
{
	// The isolated synchronization executable redirects only the production
	// DeviceResources TU's wait call here. No engine header or class is changed.
	DWORD WINAPI bounded_wait(HANDLE event, DWORD milliseconds, BOOL alertable) noexcept;
}

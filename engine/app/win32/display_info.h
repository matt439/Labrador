#pragma once

#include "engine/app/window_display_info.h"

#include <Windows.h>

namespace labrador::detail
{
	// The production query's API calls, replaceable only in platform fixtures.
	struct WindowDisplayCalls
	{
		decltype(&::MonitorFromWindow) monitor_from_window = ::MonitorFromWindow;
		decltype(&::GetMonitorInfoW) monitor_info = ::GetMonitorInfoW;
		decltype(&::EnumDisplaySettingsExW) display_settings = ::EnumDisplaySettingsExW;
	};

	// A null/closed handle makes no platform call. Each query selects the
	// window's current/nearest monitor and reads its current mode anew.
	WindowDisplayInfo query_window_display(HWND window,
		const WindowDisplayCalls& calls = {});
}

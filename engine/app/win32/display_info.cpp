#include "engine/app/win32/display_info.h"

#include "engine/render/text_encoding.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <string_view>

namespace labrador::detail
{
	WindowDisplayInfo query_window_display(HWND window,
		const WindowDisplayCalls& calls)
	{
		WindowDisplayInfo result;
		if (window == nullptr) return result;
		const HMONITOR monitor = calls.monitor_from_window(window,
			MONITOR_DEFAULTTONEAREST);
		if (monitor == nullptr) return result;

		MONITORINFOEXW info = {};
		info.cbSize = sizeof(info);
		if (calls.monitor_info(monitor, reinterpret_cast<MONITORINFO*>(&info)) == FALSE)
		{
			return result;
		}
		const wchar_t* end = std::find(std::begin(info.szDevice),
			std::end(info.szDevice), L'\0');
		if (end == std::begin(info.szDevice) || end == std::end(info.szDevice))
		{
			return result;
		}
		result.monitor_id = narrow(std::wstring_view(info.szDevice,
			static_cast<std::size_t>(end - std::begin(info.szDevice))));

		DEVMODEW mode = {};
		mode.dmSize = sizeof(mode);
		if (calls.display_settings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0) == FALSE ||
			(mode.dmFields & DM_DISPLAYFREQUENCY) == 0 || mode.dmDisplayFrequency <= 1 ||
			mode.dmDisplayFrequency > static_cast<DWORD>(std::numeric_limits<int>::max()))
		{
			return result;
		}
		result.nominal_refresh_hz = static_cast<int>(mode.dmDisplayFrequency);
		return result;
	}
}

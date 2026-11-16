#include <doctest/doctest.h>

#include "engine/app/win32/display_info.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace
{
	struct FakeDisplay
	{
		HWND window = reinterpret_cast<HWND>(std::uintptr_t{ 1 });
		HMONITOR monitor = reinterpret_cast<HMONITOR>(std::uintptr_t{ 2 });
		std::wstring device = L"\\\\.\\DISPLAY2";
		bool monitor_info_available = true;
		bool mode_available = true;
		bool frequency_field = true;
		bool unterminated_name = false;
		DWORD frequency = 60;
		std::vector<std::string> calls;
		HWND seen_window = nullptr;
		DWORD monitor_flags = 0;
		HMONITOR seen_monitor = nullptr;
		DWORD monitor_info_size = 0;
		std::wstring seen_device;
		DWORD mode_index = 0;
		DWORD mode_flags = 1;
		WORD mode_size = 0;
	};

	thread_local FakeDisplay* active = nullptr;

	HMONITOR WINAPI monitor_from_window(HWND window, DWORD flags)
	{
		active->calls.push_back("monitor");
		active->seen_window = window;
		active->monitor_flags = flags;
		return active->monitor;
	}

	BOOL WINAPI monitor_info(HMONITOR monitor, MONITORINFO* info)
	{
		active->calls.push_back("identity");
		active->seen_monitor = monitor;
		active->monitor_info_size = info->cbSize;
		if (!active->monitor_info_available) return FALSE;
		MONITORINFOEXW& extended = *reinterpret_cast<MONITORINFOEXW*>(info);
		std::fill(std::begin(extended.szDevice), std::end(extended.szDevice),
			active->unterminated_name ? L'x' : L'\0');
		if (!active->unterminated_name)
		{
			std::copy_n(active->device.data(), std::min(active->device.size(),
				std::size(extended.szDevice) - 1), extended.szDevice);
		}
		return TRUE;
	}

	BOOL WINAPI display_settings(LPCWSTR device, DWORD index, DEVMODEW* mode, DWORD flags)
	{
		active->calls.push_back("mode");
		active->seen_device = device == nullptr ? L"" : device;
		active->mode_index = index;
		active->mode_flags = flags;
		active->mode_size = mode->dmSize;
		if (!active->mode_available) return FALSE;
		mode->dmFields = active->frequency_field ? DM_DISPLAYFREQUENCY : 0;
		mode->dmDisplayFrequency = active->frequency;
		return TRUE;
	}

	class Fixture
	{
	public:
		Fixture() { active = &this->state; }
		~Fixture() { active = nullptr; }
		Fixture(const Fixture&) = delete;
		Fixture& operator=(const Fixture&) = delete;
		labrador::WindowDisplayInfo query()
		{
			return labrador::detail::query_window_display(this->state.window,
				{ monitor_from_window, monitor_info, display_settings });
		}
		FakeDisplay state;
	};
}

TEST_CASE("display metadata selects the window's nearest monitor and its named current mode")
{
	Fixture fixture;
	const labrador::WindowDisplayInfo info = fixture.query();
	CHECK(fixture.state.calls == std::vector<std::string>{ "monitor", "identity", "mode" });
	CHECK(fixture.state.seen_window == fixture.state.window);
	CHECK(fixture.state.monitor_flags == MONITOR_DEFAULTTONEAREST);
	CHECK(fixture.state.seen_monitor == fixture.state.monitor);
	CHECK(fixture.state.monitor_info_size == sizeof(MONITORINFOEXW));
	CHECK(fixture.state.seen_device == fixture.state.device);
	CHECK(fixture.state.mode_index == ENUM_CURRENT_SETTINGS);
	CHECK(fixture.state.mode_flags == 0);
	CHECK(fixture.state.mode_size == sizeof(DEVMODEW));
	CHECK(info.monitor_id == "\\\\.\\DISPLAY2");
	REQUIRE(info.nominal_refresh_hz.has_value());
	CHECK(*info.nominal_refresh_hz == 60);
}

TEST_CASE("each display query reflects monitor moves mode changes and later lookup failure")
{
	Fixture fixture;
	const labrador::WindowDisplayInfo first = fixture.query();
	fixture.state.monitor = reinterpret_cast<HMONITOR>(std::uintptr_t{ 3 });
	fixture.state.device = L"\\\\.\\DISPLAY3";
	fixture.state.frequency = 144;
	const labrador::WindowDisplayInfo moved = fixture.query();
	CHECK(moved.monitor_id == "\\\\.\\DISPLAY3");
	CHECK(moved.nominal_refresh_hz == 144);
	fixture.state.frequency = 59;
	CHECK(fixture.query().nominal_refresh_hz == 59);
	fixture.state.mode_available = false;
	const labrador::WindowDisplayInfo unavailable = fixture.query();
	CHECK(unavailable.monitor_id == "\\\\.\\DISPLAY3");
	CHECK_FALSE(unavailable.nominal_refresh_hz.has_value());
	CHECK(first.monitor_id == "\\\\.\\DISPLAY2");
	CHECK(first.nominal_refresh_hz == 60);
}

TEST_CASE("display query failures and absent window identity stay explicitly unknown")
{
	Fixture fixture;
	SUBCASE("a null window never queries a primary or nearest monitor")
	{
		fixture.state.window = nullptr;
		CHECK(fixture.query().monitor_id.empty());
		CHECK(fixture.state.calls.empty());
	}
	SUBCASE("no monitor") { fixture.state.monitor = nullptr; }
	SUBCASE("monitor identity query failed") { fixture.state.monitor_info_available = false; }
	SUBCASE("empty device name") { fixture.state.device.clear(); }
	SUBCASE("unterminated device name") { fixture.state.unterminated_name = true; }
	const labrador::WindowDisplayInfo info = fixture.query();
	CHECK(info.monitor_id.empty());
	CHECK_FALSE(info.nominal_refresh_hz.has_value());
	CHECK(std::find(fixture.state.calls.begin(), fixture.state.calls.end(), "mode") == fixture.state.calls.end());
}

TEST_CASE("native default missing and unrepresentable refresh values do not become guessed rates")
{
	Fixture fixture;
	for (const DWORD frequency : { DWORD{ 0 }, DWORD{ 1 }, std::numeric_limits<DWORD>::max(),
		static_cast<DWORD>(std::numeric_limits<int>::max()) + 1 })
	{
		fixture.state.frequency = frequency;
		CHECK_FALSE(fixture.query().nominal_refresh_hz.has_value());
	}
	fixture.state.frequency = 60;
	fixture.state.frequency_field = false;
	CHECK_FALSE(fixture.query().nominal_refresh_hz.has_value());
	fixture.state.frequency_field = true;
	for (const DWORD frequency : { DWORD{ 2 }, DWORD{ 59 }, DWORD{ 144 }, DWORD{ 1200 },
		static_cast<DWORD>(std::numeric_limits<int>::max()) })
	{
		fixture.state.frequency = frequency;
		CHECK(fixture.query().nominal_refresh_hz == static_cast<int>(frequency));
	}
}

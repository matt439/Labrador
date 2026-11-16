#include <doctest/doctest.h>

#include "tests/app/startup_child_exit_codes.h"

#include <Windows.h>
#include <filesystem>
#include <string>

// window.cpp's first ShowWindow passes SW_SHOWDEFAULT, which asks for the show
// command the launching process put in STARTUPINFO. A launcher that starts a
// game hidden - PowerShell's Start-Process -WindowStyle Hidden is one - relies
// on that, so these cases launch startup_child.cpp both ways and read back
// whether its window was shown. Native-null also runs an Application child
// through the first completed frame to test the deferred-show path itself.

namespace
{
	// Runs the child with `show` as STARTUPINFO's show command and answers
	// its exit code.
	DWORD run_child(WORD show, const char* executable = LABRADOR_STARTUP_CHILD,
		const wchar_t* arguments = L"")
	{
		STARTUPINFOW startup = {};
		startup.cb = sizeof(STARTUPINFOW);
		startup.dwFlags = STARTF_USESHOWWINDOW;
		startup.wShowWindow = show;

		std::wstring command = L"\"" +
			std::filesystem::path(executable).wstring() + L"\" " + arguments;
		PROCESS_INFORMATION process = {};
		REQUIRE(CreateProcessW(nullptr, command.data(), nullptr, nullptr,
			FALSE, 0, nullptr, nullptr, &startup, &process) != 0);

		const DWORD waited = WaitForSingleObject(process.hProcess, 30000);
		DWORD code = 0;
		const BOOL read = GetExitCodeProcess(process.hProcess, &code);
		if (waited != WAIT_OBJECT_0)
		{
			TerminateProcess(process.hProcess, 1);
		}
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);

		REQUIRE(waited == WAIT_OBJECT_0);
		REQUIRE(read != 0);
		return code;
	}
}

TEST_CASE("a game launched with SW_HIDE in STARTUPINFO keeps its window hidden")
{
	CHECK(run_child(SW_HIDE) == static_cast<DWORD>(startup_child_hidden));
}

TEST_CASE("a game launched with SW_SHOWNORMAL in STARTUPINFO shows its window")
{
	// The other half, so the case above cannot pass by never showing.
	CHECK(run_child(SW_SHOWNORMAL) == static_cast<DWORD>(startup_child_visible));
}

#if defined(LABRADOR_APPLICATION_STARTUP_CHILD)
TEST_CASE("Application's first-frame reveal honors the launcher's STARTUPINFO")
{
	CHECK(run_child(SW_HIDE, LABRADOR_APPLICATION_STARTUP_CHILD) ==
		static_cast<DWORD>(startup_child_hidden));
	CHECK(run_child(SW_SHOWNORMAL, LABRADOR_APPLICATION_STARTUP_CHILD) ==
		static_cast<DWORD>(startup_child_visible));
	CHECK(run_child(SW_SHOWMAXIMIZED, LABRADOR_APPLICATION_STARTUP_CHILD) ==
		static_cast<DWORD>(startup_child_maximized));
	CHECK(run_child(SW_SHOWMINIMIZED, LABRADOR_APPLICATION_STARTUP_CHILD) ==
		static_cast<DWORD>(startup_child_minimized));
	CHECK(run_child(SW_SHOWMAXIMIZED, LABRADOR_APPLICATION_STARTUP_CHILD,
		L"fullscreen-to-windowed") == static_cast<DWORD>(startup_child_maximized));
	CHECK(run_child(SW_SHOWMAXIMIZED, LABRADOR_APPLICATION_STARTUP_CHILD,
		L"fullscreen-round-trip") == static_cast<DWORD>(startup_child_maximized));
}

TEST_CASE("explicit hidden Application startup takes precedence over the launcher")
{
	CHECK(run_child(SW_SHOWNORMAL, LABRADOR_APPLICATION_STARTUP_CHILD, L"hidden") ==
		static_cast<DWORD>(startup_child_hidden));
	CHECK(run_child(SW_SHOWMAXIMIZED, LABRADOR_APPLICATION_STARTUP_CHILD, L"hidden") ==
		static_cast<DWORD>(startup_child_hidden));
}
#endif

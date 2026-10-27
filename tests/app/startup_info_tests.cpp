#include <doctest/doctest.h>

#include <Windows.h>
#include <filesystem>
#include <string>

// window.cpp's first ShowWindow passes SW_SHOWDEFAULT, which asks for the show
// command the launching process put in STARTUPINFO. A launcher that starts a
// game hidden - PowerShell's Start-Process -WindowStyle Hidden is one - relies
// on that, so these cases launch startup_child.cpp both ways and read back
// whether its window was shown. The child makes its window as Application
// does from default ApplicationOptions, with no device.

namespace
{
	// startup_child.cpp's exit codes.
	constexpr DWORD child_visible = 10;
	constexpr DWORD child_hidden = 20;

	// Runs the child with `show` as STARTUPINFO's show command and answers
	// its exit code.
	DWORD run_child(WORD show)
	{
		STARTUPINFOW startup = {};
		startup.cb = sizeof(STARTUPINFOW);
		startup.dwFlags = STARTF_USESHOWWINDOW;
		startup.wShowWindow = show;

		std::wstring command = L"\"" +
			std::filesystem::path(LABRADOR_STARTUP_CHILD).wstring() + L"\"";
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
	CHECK(run_child(SW_HIDE) == child_hidden);
}

TEST_CASE("a game launched with SW_SHOWNORMAL in STARTUPINFO shows its window")
{
	// The other half, so the case above cannot pass by never showing.
	CHECK(run_child(SW_SHOWNORMAL) == child_visible);
}

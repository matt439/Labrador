// Windows host helper for the Firefox page tests. The supervisor owns the
// only handle to a private job. Firefox joins at creation, before any of
// its code can run or spawn a child. A closed control pipe requests a stop.
// If the supervisor itself exits, the kernel closes the job handle.
#include <windows.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{
	struct CloseHandleDeleter
	{
		void operator()(HANDLE handle) const { CloseHandle(handle); }
	};

	using Handle = std::unique_ptr<void, CloseHandleDeleter>;

	struct AttributeListGuard
	{
		LPPROC_THREAD_ATTRIBUTE_LIST list;
		~AttributeListGuard() { DeleteProcThreadAttributeList(list); }
	};

	int error(const char* operation)
	{
		const DWORD code = GetLastError();
		std::fprintf(stderr, "firefox_job: %s failed (Win32 %lu)\n", operation, code);
		return 1;
	}

	// Windows supplies one command-line string. Remove only our own executable
	// token and preserve the caller's quoting of every forwarded argument.
	std::wstring browser_command_line()
	{
		const wchar_t* command = GetCommandLineW();
		bool quoted = false;
		while (*command && (quoted || (*command != L' ' && *command != L'\t')))
		{
			if (*command == L'"') quoted = !quoted;
			++command;
		}
		while (*command == L' ' || *command == L'\t') ++command;
		return command;
	}

	Handle child_output(DWORD standard_handle, HANDLE null_device)
	{
		HANDLE source = GetStdHandle(standard_handle);
		if (!source || source == INVALID_HANDLE_VALUE) source = null_device;
		HANDLE copy = nullptr;
		if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
			&copy, 0, TRUE, DUPLICATE_SAME_ACCESS)) return {};
		return Handle(copy);
	}

	bool launch(HANDLE job, const wchar_t* executable, Handle& launcher)
	{
		SECURITY_ATTRIBUTES security{ sizeof(security), nullptr, TRUE };
		HANDLE device = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
		if (device == INVALID_HANDLE_VALUE) { error("opening NUL"); return false; }
		const Handle null_device(device);
		const Handle output = child_output(STD_OUTPUT_HANDLE, device);
		if (!output) { error("duplicating stdout"); return false; }
		const Handle errors = child_output(STD_ERROR_HANDLE, device);
		if (!errors) { error("duplicating stderr"); return false; }
		HANDLE inherited[] = { device, output.get(), errors.get() };

		SIZE_T size = 0;
		InitializeProcThreadAttributeList(nullptr, 2, 0, &size);
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
		{
			error("sizing process attributes");
			return false;
		}
		std::vector<unsigned char> storage(size);
		LPPROC_THREAD_ATTRIBUTE_LIST attributes =
			reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
		if (!InitializeProcThreadAttributeList(attributes, 2, 0, &size))
		{
			error("initializing process attributes");
			return false;
		}
		const AttributeListGuard list{ attributes };
		if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
			&job, sizeof(job), nullptr, nullptr))
		{
			error("setting the job list");
			return false;
		}
		if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
			inherited, sizeof(inherited), nullptr, nullptr))
		{
			error("setting inherited standard handles");
			return false;
		}

		STARTUPINFOEXW startup{};
		startup.StartupInfo.cb = sizeof(startup);
		startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		startup.StartupInfo.wShowWindow = SW_HIDE;
		startup.StartupInfo.hStdInput = device;
		startup.StartupInfo.hStdOutput = output.get();
		startup.StartupInfo.hStdError = errors.get();
		startup.lpAttributeList = attributes;
		PROCESS_INFORMATION process{};
		std::wstring arguments = browser_command_line();
		if (!CreateProcessW(executable, arguments.data(), nullptr, nullptr, TRUE,
			EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process))
		{
			error("creating the browser in its job");
			return false;
		}
		CloseHandle(process.hThread);
		launcher.reset(process.hProcess);
		return true;
	}
}

int wmain(int argc, wchar_t* argv[])
{
	const HANDLE control = GetStdHandle(STD_INPUT_HANDLE);
	if (argc < 2 || GetFileType(control) != FILE_TYPE_PIPE)
	{
		std::fprintf(stderr, "firefox_job: expected a browser executable and a control pipe on stdin\n");
		return 1;
	}
	const Handle job(CreateJobObjectW(nullptr, nullptr));
	if (!job) return error("creating private job");
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
		return error("setting kill-on-close");

	Handle launcher;
	if (!launch(job.get(), argv[1], launcher)) return 1;
	bool stopping = false;
	ULONGLONG stop_started = 0;
	DWORD result = 0;
	for (;;)
	{
		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
		if (!QueryInformationJobObject(job.get(), JobObjectBasicAccountingInformation,
			&accounting, sizeof(accounting), nullptr)) return error("querying active process count");
		if (accounting.ActiveProcesses == 0)
		{
			const DWORD state = WaitForSingleObject(launcher.get(), 1000);
			if (state == WAIT_FAILED) return error("confirming launcher exit");
			if (state != WAIT_OBJECT_0)
			{
				std::fprintf(stderr, "firefox_job: job empty but launcher exit unconfirmed (wait %lu)\n", state);
				return 1;
			}
			if (!stopping && !GetExitCodeProcess(launcher.get(), &result))
				return error("reading launcher exit code");
			if (stopping) std::fprintf(stderr, "firefox_job: owned job is empty after stop\n");
			return static_cast<int>(result);
		}
		if (!stopping)
		{
			DWORD available = 0;
			if (!PeekNamedPipe(control, nullptr, 0, nullptr, &available, nullptr))
			{
				const DWORD reason = GetLastError();
				if (reason != ERROR_BROKEN_PIPE && reason != ERROR_PIPE_NOT_CONNECTED &&
					reason != ERROR_NO_DATA) return error("reading control pipe");
				stopping = true;
			}
			else if (available != 0) stopping = true;
			if (stopping)
			{
				// Retain a launcher failure that preceded a request to stop its
				// remaining descendants. A live launcher is stopped by request.
				const DWORD state = WaitForSingleObject(launcher.get(), 0);
				if (state == WAIT_FAILED) return error("checking launcher before stop");
				if (state == WAIT_OBJECT_0 &&
					!GetExitCodeProcess(launcher.get(), &result)) return error("reading launcher exit code");
				if (!TerminateJobObject(job.get(), 1)) return error("terminating the owned job");
				stop_started = GetTickCount64();
			}
		}
		else if (GetTickCount64() - stop_started >= 5000)
		{
			std::fprintf(stderr, "firefox_job: %lu owned processes remain five seconds after stop\n",
				accounting.ActiveProcesses);
			return 1;
		}
		Sleep(25);
	}
}

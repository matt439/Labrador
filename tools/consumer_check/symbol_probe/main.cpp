#include <windows.h>
#include <dbghelp.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	using Clock = std::chrono::steady_clock;

	std::string utf8(const std::wstring& text)
	{
		if (text.empty()) return {};
		const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (count == 0) throw std::runtime_error("Cannot encode a Windows path.");
		std::string result(static_cast<std::size_t>(count), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
		result.pop_back();
		return result;
	}

	[[noreturn]] void fail_windows(const char* operation)
	{
		throw std::runtime_error(std::string(operation) + " failed, Windows error " +
			std::to_string(GetLastError()) + ".");
	}

	void require(bool condition, const std::string& message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	std::wstring environment(const wchar_t* name)
	{
		const DWORD count = GetEnvironmentVariableW(name, nullptr, 0);
		if (count == 0) return {};
		std::wstring value(count, L'\0');
		const DWORD written = GetEnvironmentVariableW(name, value.data(), count);
		require(written < count, "An environment value changed while it was read.");
		value.resize(written);
		return value;
	}

	struct Options
	{
		std::filesystem::path exe;
		std::filesystem::path pdb;
		std::filesystem::path source;
		std::filesystem::path dbghelp;
		std::wstring symbol = L"wWinMain";
		std::chrono::seconds timeout{60};
	};

	Options arguments(int count, wchar_t** values)
	{
		Options result;
		for (int index = 1; index < count; index += 2)
		{
			require(index + 1 < count, "Every option needs a value.");
			const std::wstring name(values[index]);
			const std::wstring value(values[index + 1]);
			if (name == L"--exe") result.exe = value;
			else if (name == L"--pdb") result.pdb = value;
			else if (name == L"--source") result.source = value;
			else if (name == L"--dbghelp") result.dbghelp = value;
			else if (name == L"--symbol") result.symbol = value;
			else if (name == L"--timeout")
			{
				std::size_t consumed = 0;
				const int seconds = std::stoi(value, &consumed);
				require(consumed == value.size() && seconds > 0 && seconds <= 3600,
					"--timeout must be from 1 to 3600 seconds.");
				result.timeout = std::chrono::seconds(seconds);
			}
			else throw std::runtime_error("Unknown option: " + utf8(name));
		}
		require(!result.exe.empty() && !result.pdb.empty() && !result.source.empty(),
			"Required: --exe <game.exe> --pdb <game.pdb> --source <preserved main.cpp>.");
		require(!result.symbol.empty(), "--symbol must not be empty.");
		if (result.dbghelp.empty())
		{
			const std::wstring sdk = environment(L"WindowsSdkDir");
			result.dbghelp = sdk.empty()
				? std::filesystem::path(environment(L"ProgramFiles(x86)")) / L"Windows Kits/10/Debuggers/x64/dbghelp.dll"
				: std::filesystem::path(sdk) / L"Debuggers/x64/dbghelp.dll";
		}
		for (std::filesystem::path* path : { &result.exe, &result.pdb, &result.source, &result.dbghelp })
		{
			*path = std::filesystem::canonical(*path);
			require(std::filesystem::is_regular_file(*path), "Not a regular file: " + utf8(path->wstring()));
		}
		return result;
	}

	std::string hex(const BYTE* bytes, std::size_t count)
	{
		std::ostringstream output;
		output << std::hex << std::setfill('0');
		for (std::size_t index = 0; index < count; ++index)
			output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
		return output.str();
	}

	std::array<BYTE, 32> sha256(const std::filesystem::path& path)
	{
		struct HashHandles
		{
			BCRYPT_ALG_HANDLE algorithm = nullptr;
			BCRYPT_HASH_HANDLE hash = nullptr;
			~HashHandles()
			{
				if (hash) BCryptDestroyHash(hash);
				if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
			}
		} handles;
		require(BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0,
			"Cannot open SHA-256 provider.");
		DWORD object_size = 0;
		DWORD written = 0;
		require(BCryptGetProperty(handles.algorithm, BCRYPT_OBJECT_LENGTH,
			reinterpret_cast<BYTE*>(&object_size), sizeof(object_size), &written, 0) >= 0,
			"Cannot query SHA-256 storage.");
		std::vector<BYTE> object(object_size);
		require(BCryptCreateHash(handles.algorithm, &handles.hash, object.data(), object_size, nullptr, 0, 0) >= 0,
			"Cannot create SHA-256 hash.");
		std::ifstream input(path, std::ios::binary);
		require(input.is_open(), "Cannot read " + utf8(path.wstring()));
		std::array<char, 65536> buffer{};
		while (input)
		{
			input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			const ULONG size = static_cast<ULONG>(input.gcount());
			require(BCryptHashData(handles.hash, reinterpret_cast<BYTE*>(buffer.data()), size, 0) >= 0,
				"Cannot hash file bytes.");
		}
		require(input.eof(), "File read failed: " + utf8(path.wstring()));
		std::array<BYTE, 32> result{};
		require(BCryptFinishHash(handles.hash, result.data(), static_cast<ULONG>(result.size()), 0) >= 0,
			"Cannot finish SHA-256 hash.");
		return result;
	}

	template<typename T>
	T read(const std::vector<char>& bytes, std::size_t offset)
	{
		require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset, "Truncated PE record.");
		T value{};
		std::memcpy(&value, bytes.data() + offset, sizeof(T));
		return value;
	}

	struct PdbIdentity { GUID guid; DWORD age; std::string recorded_path; };

	PdbIdentity image_identity(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		require(input.is_open(), "Cannot read the executable.");
		const std::streamoff length = input.tellg();
		require(length > 0 && length <= std::numeric_limits<std::uint32_t>::max(), "Unsupported PE file size.");
		std::vector<char> bytes(static_cast<std::size_t>(length));
		input.seekg(0);
		input.read(bytes.data(), length);
		require(static_cast<bool>(input), "Cannot read all executable bytes.");
		const IMAGE_DOS_HEADER dos = read<IMAGE_DOS_HEADER>(bytes, 0);
		require(dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= 0, "Not a PE executable.");
		const std::size_t header = static_cast<std::size_t>(dos.e_lfanew);
		require(read<DWORD>(bytes, header) == IMAGE_NT_SIGNATURE, "Invalid PE signature.");
		const IMAGE_FILE_HEADER file = read<IMAGE_FILE_HEADER>(bytes, header + sizeof(DWORD));
		require(file.Machine == IMAGE_FILE_MACHINE_AMD64, "The probe supports x64 consumers only.");
		require(file.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64), "Truncated PE optional header.");
		const std::size_t optional_offset = header + sizeof(DWORD) + sizeof(file);
		const IMAGE_OPTIONAL_HEADER64 optional = read<IMAGE_OPTIONAL_HEADER64>(bytes, optional_offset);
		require(optional.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
			optional.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG, "Missing PE debug directory.");
		const IMAGE_DATA_DIRECTORY directory = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
		const std::size_t sections = optional_offset + file.SizeOfOptionalHeader;
		std::size_t directory_offset = 0;
		for (WORD index = 0; index < file.NumberOfSections; ++index)
		{
			const IMAGE_SECTION_HEADER section = read<IMAGE_SECTION_HEADER>(bytes,
				sections + static_cast<std::size_t>(index) * sizeof(IMAGE_SECTION_HEADER));
			if (directory.VirtualAddress >= section.VirtualAddress &&
				static_cast<std::uint64_t>(directory.VirtualAddress) - section.VirtualAddress < section.SizeOfRawData)
			{
				directory_offset = static_cast<std::size_t>(section.PointerToRawData) +
					directory.VirtualAddress - section.VirtualAddress;
				break;
			}
		}
		require(directory_offset != 0 && directory.Size % sizeof(IMAGE_DEBUG_DIRECTORY) == 0,
			"Cannot locate the PE debug directory.");
		for (std::size_t offset = 0; offset < directory.Size; offset += sizeof(IMAGE_DEBUG_DIRECTORY))
		{
			const IMAGE_DEBUG_DIRECTORY entry = read<IMAGE_DEBUG_DIRECTORY>(bytes, directory_offset + offset);
			if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData < 25) continue;
			const std::size_t record = entry.PointerToRawData;
			require(record <= bytes.size() && entry.SizeOfData <= bytes.size() - record, "Truncated CodeView record.");
			if (std::memcmp(bytes.data() + record, "RSDS", 4) != 0) continue;
			const char* begin = bytes.data() + record + 24;
			const char* end = bytes.data() + record + entry.SizeOfData;
			const char* terminator = std::find(begin, end, '\0');
			require(terminator != end, "Unterminated CodeView PDB path.");
			return { read<GUID>(bytes, record + 4), read<DWORD>(bytes, record + 20), std::string(begin, terminator) };
		}
		throw std::runtime_error("The executable has no RSDS PDB identity; build with /DEBUG:FULL.");
	}

	class Symbols
	{
	public:
		explicit Symbols(const std::filesystem::path& path)
		{
			library_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (!library_) fail_windows("LoadLibraryExW(DbgHelp)");
			std::array<wchar_t, 32768> loaded{};
			const DWORD count = GetModuleFileNameW(library_, loaded.data(), static_cast<DWORD>(loaded.size()));
			require(count > 0 && count < loaded.size(), "Cannot identify loaded DbgHelp.");
			std::cout << "dbghelp=" << utf8(std::wstring(loaded.data(), count)) << '\n';
		}
		Symbols(const Symbols&) = delete;
		Symbols& operator=(const Symbols&) = delete;
		~Symbols()
		{
			if (process_ && cleanup_) cleanup_(process_);
			if (library_) FreeLibrary(library_);
		}
		template<typename Function> Function function(const char* name) const
		{
			const FARPROC address = GetProcAddress(library_, name);
			if (!address) fail_windows(name);
			return std::bit_cast<Function>(address);
		}
		void initialize(HANDLE process, const std::filesystem::path& directory)
		{
			cleanup_ = function<decltype(&SymCleanup)>("SymCleanup");
			const DWORD options = SYMOPT_EXACT_SYMBOLS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME |
				SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_IGNORE_NT_SYMPATH |
				SYMOPT_IGNORE_CVREC | SYMOPT_IGNORE_IMAGEDIR | SYMOPT_NO_IMAGE_SEARCH |
				SYMOPT_DISABLE_SYMSRV_AUTODETECT;
			function<decltype(&SymSetOptions)>("SymSetOptions")(options);
			if (!function<decltype(&SymInitializeW)>("SymInitializeW")(process, directory.c_str(), FALSE))
				fail_windows("SymInitializeW");
			process_ = process;
		}
	private:
		HMODULE library_ = nullptr;
		HANDLE process_ = nullptr;
		decltype(&SymCleanup) cleanup_ = nullptr;
	};

	class Child
	{
	public:
		explicit Child(const Options& options)
		{
			std::wstring command = L"\"" + options.exe.wstring() + L"\" --smoke-test";
			STARTUPINFOW startup{};
			startup.cb = sizeof(startup);
			if (!CreateProcessW(options.exe.c_str(), command.data(), nullptr, nullptr, FALSE,
				DEBUG_ONLY_THIS_PROCESS, nullptr, options.exe.parent_path().c_str(), &startup, &process_))
				fail_windows("CreateProcessW");
			active_ = true;
			std::cout << "consumer_pid=" << process_.dwProcessId << '\n';
		}
		Child(const Child&) = delete;
		Child& operator=(const Child&) = delete;
		~Child()
		{
			if (active_)
			{
				TerminateProcess(process_.hProcess, 1);
				if (pending_)
				{
					close_image(event_);
					ContinueDebugEvent(event_.dwProcessId, event_.dwThreadId, DBG_CONTINUE);
				}
				const Clock::time_point deadline = Clock::now() + std::chrono::seconds(5);
				while (Clock::now() < deadline)
				{
					DEBUG_EVENT event{};
					if (!WaitForDebugEventEx(&event, 100)) continue;
					close_image(event);
					ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
					if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) break;
				}
			}
			if (process_.hThread) CloseHandle(process_.hThread);
			if (process_.hProcess) CloseHandle(process_.hProcess);
		}
		static void close_image(DEBUG_EVENT& event)
		{
			HANDLE file = nullptr;
			if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
			{
				file = event.u.CreateProcessInfo.hFile;
				event.u.CreateProcessInfo.hFile = nullptr;
			}
			if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
			{
				file = event.u.LoadDll.hFile;
				event.u.LoadDll.hFile = nullptr;
			}
			if (file) CloseHandle(file);
		}
		const DEBUG_EVENT& wait(DWORD milliseconds)
		{
			if (!WaitForDebugEventEx(&event_, milliseconds)) fail_windows("WaitForDebugEventEx (deadline)");
			pending_ = true;
			return event_;
		}
		void resume(DWORD status)
		{
			close_image(event_);
			if (!ContinueDebugEvent(event_.dwProcessId, event_.dwThreadId, status)) fail_windows("ContinueDebugEvent");
			pending_ = false;
			if (event_.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) active_ = false;
		}
		HANDLE process() const { return process_.hProcess; }
	private:
		PROCESS_INFORMATION process_{};
		DEBUG_EVENT event_{};
		bool active_ = false;
		bool pending_ = false;
	};

	struct SourceLine { DWORD64 address; DWORD line; std::wstring file; };
	struct LineSearch { DWORD64 begin; DWORD64 end; std::vector<SourceLine> lines; bool failed = false; };

	BOOL CALLBACK collect_line(PSRCCODEINFOW line, PVOID context)
	{
		LineSearch& search = *static_cast<LineSearch*>(context);
		if (line->LineNumber > 0 && line->Address >= search.begin && line->Address < search.end)
		{
			try { search.lines.push_back({ line->Address, line->LineNumber, line->FileName }); }
			catch (...) { search.failed = true; return FALSE; }
		}
		return TRUE;
	}

	SourceLine load_symbols(Symbols& symbols, HANDLE process, HANDLE image_file, DWORD64 base,
		const Options& options, const PdbIdentity& expected, const std::array<BYTE, 32>& source_hash)
	{
		if (!symbols.function<decltype(&SymLoadModuleExW)>("SymLoadModuleExW")(
			process, image_file, options.exe.c_str(), L"consumer", base, 0, nullptr, 0))
			fail_windows("SymLoadModuleExW");
		IMAGEHLP_MODULEW64 module{};
		module.SizeOfStruct = sizeof(module);
		if (!symbols.function<decltype(&SymGetModuleInfoW64)>("SymGetModuleInfoW64")(process, base, &module))
			fail_windows("SymGetModuleInfoW64");
		require(module.SymType == SymPdb && module.LineNumbers && !module.PdbUnmatched,
			"Exact PDB with source lines was not loaded; check /Zi, /DEBUG:FULL and the supplied PDB.");
		require(std::filesystem::equivalent(options.pdb, module.LoadedPdbName), "DbgHelp loaded a different PDB path.");
		require(std::memcmp(&module.PdbSig70, &expected.guid, sizeof(GUID)) == 0 && module.PdbAge == expected.age,
			"Loaded PDB GUID/age does not match the executable's RSDS record.");
		std::cout << "pdb_loaded=" << utf8(module.LoadedPdbName) << "\npdb_guid_bytes=" <<
			hex(reinterpret_cast<const BYTE*>(&module.PdbSig70), sizeof(GUID)) << "\npdb_age=" << module.PdbAge << '\n';
		alignas(SYMBOL_INFOW) std::array<BYTE, sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)> storage{};
		SYMBOL_INFOW* symbol = reinterpret_cast<SYMBOL_INFOW*>(storage.data());
		symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
		symbol->MaxNameLen = MAX_SYM_NAME;
		const std::wstring name = L"consumer!" + options.symbol;
		if (!symbols.function<decltype(&SymFromNameW)>("SymFromNameW")(process, name.c_str(), symbol))
			fail_windows("SymFromNameW");
		require(symbol->Size > 0 && symbol->Address >= base && symbol->Address <=
			std::numeric_limits<DWORD64>::max() - symbol->Size, "The requested function has no bounded code range.");
		LineSearch search{ symbol->Address, symbol->Address + symbol->Size, {} };
		if (!symbols.function<decltype(&SymEnumLinesW)>("SymEnumLinesW")(process, base, nullptr, nullptr, collect_line, &search))
			fail_windows("SymEnumLinesW");
		require(!search.failed, "Could not retain the requested source lines.");
		std::sort(search.lines.begin(), search.lines.end(), [](const SourceLine& a, const SourceLine& b) { return a.address < b.address; });
		for (const SourceLine& line : search.lines)
		{
			DWORD kind = 0;
			DWORD count = 0;
			std::array<BYTE, 64> checksum{};
			if (symbols.function<decltype(&SymGetSourceFileChecksumW)>("SymGetSourceFileChecksumW")(
				process, base, line.file.c_str(), &kind, checksum.data(), static_cast<DWORD>(checksum.size()), &count) &&
				kind == 3 && count == source_hash.size() && std::equal(source_hash.begin(), source_hash.end(), checksum.begin()))
			{
				std::cout << "symbol=" << utf8(options.symbol) << "\nsource_record=" << utf8(line.file) <<
					"\nsource_line=" << line.line << "\nsource_checksum=SHA256:" << hex(source_hash.data(), source_hash.size()) << '\n';
				return line;
			}
		}
		throw std::runtime_error("No source line in the requested function matches the preserved source SHA-256. Use /Zi /ZH:SHA_256 and the exact source bytes.");
	}

	BYTE replace_byte(HANDLE process, DWORD64 address, BYTE replacement)
	{
		BYTE original = 0;
		SIZE_T count = 0;
		LPVOID location = reinterpret_cast<LPVOID>(static_cast<std::uintptr_t>(address));
		if (!ReadProcessMemory(process, location, &original, 1, &count) || count != 1) fail_windows("ReadProcessMemory");
		DWORD protection = 0;
		if (!VirtualProtectEx(process, location, 1, PAGE_EXECUTE_READWRITE, &protection)) fail_windows("VirtualProtectEx");
		const BOOL wrote = WriteProcessMemory(process, location, &replacement, 1, &count);
		DWORD ignored = 0;
		const BOOL restored = VirtualProtectEx(process, location, 1, protection, &ignored);
		if (!wrote || count != 1 || !restored || !FlushInstructionCache(process, location, 1)) fail_windows("Write/restore breakpoint");
		return original;
	}

	int run(const Options& options)
	{
		const PdbIdentity expected = image_identity(options.exe);
		require(_stricmp(std::filesystem::path(expected.recorded_path).filename().string().c_str(),
			options.pdb.filename().string().c_str()) == 0, "Keep the PDB's recorded basename when relocating it.");
		const std::array<BYTE, 32> source_hash = sha256(options.source);
		for (const std::filesystem::path* path : { &options.exe, &options.pdb, &options.source })
		{
			const std::array<BYTE, 32> hash = sha256(*path);
			std::cout << "file=" << utf8(path->wstring()) << "\nsha256=" << hex(hash.data(), hash.size()) << '\n';
		}
		Child child(options);
		Symbols symbols(options.dbghelp);
		symbols.initialize(child.process(), options.pdb.parent_path());
		const Clock::time_point deadline = Clock::now() + options.timeout;
		DWORD64 breakpoint = 0;
		BYTE original = 0;
		bool hit = false;
		bool loader_breakpoint = false;
		while (Clock::now() < deadline)
		{
			const std::chrono::milliseconds remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
			const DWORD wait = static_cast<DWORD>(std::max<std::int64_t>(1, remaining.count()));
			const DEBUG_EVENT& event = child.wait(wait);
			DWORD continuation = DBG_CONTINUE;
			if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
			{
				const DWORD64 base = reinterpret_cast<std::uintptr_t>(event.u.CreateProcessInfo.lpBaseOfImage);
				const SourceLine line = load_symbols(symbols, child.process(), event.u.CreateProcessInfo.hFile,
					base, options, expected, source_hash);
				breakpoint = line.address;
				original = replace_byte(child.process(), breakpoint, 0xcc);
			}
			else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
			{
				const EXCEPTION_RECORD& exception = event.u.Exception.ExceptionRecord;
				const DWORD64 address = reinterpret_cast<std::uintptr_t>(exception.ExceptionAddress);
				if (exception.ExceptionCode == EXCEPTION_BREAKPOINT && address == breakpoint && !hit)
				{
					replace_byte(child.process(), breakpoint, original);
					CONTEXT context{};
					context.ContextFlags = CONTEXT_CONTROL;
					HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, event.dwThreadId);
					if (!thread) fail_windows("OpenThread");
					const BOOL got = GetThreadContext(thread, &context);
					if (got) context.Rip = breakpoint;
					const BOOL set = got && SetThreadContext(thread, &context);
					CloseHandle(thread);
					if (!set) fail_windows("Get/SetThreadContext");
					hit = true;
					std::cout << "breakpoint_hit=0x" << std::hex << breakpoint << std::dec << '\n';
				}
				else if (exception.ExceptionCode == EXCEPTION_BREAKPOINT && !loader_breakpoint && !hit)
				{
					loader_breakpoint = true;
				}
				else continuation = DBG_EXCEPTION_NOT_HANDLED;
			}
			const bool exited = event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT;
			const DWORD exit_code = exited ? event.u.ExitProcess.dwExitCode : 0;
			child.resume(continuation);
			if (exited)
			{
				require(hit, "The consumer exited before the source breakpoint was hit.");
				require(exit_code == 0, "Consumer smoke test failed, exit " + std::to_string(exit_code) + ".");
				std::cout << "consumer_exit=0\nPASS exact PDB, preserved source, breakpoint and smoke exit\n";
				return 0;
			}
		}
		throw std::runtime_error("The consumer exceeded --timeout.");
	}
}

int wmain(int count, wchar_t** values)
{
	try { return run(arguments(count, values)); }
	catch (const std::exception& error)
	{
		std::cerr << "symbol_probe: " << error.what() << '\n';
		return 1;
	}
}

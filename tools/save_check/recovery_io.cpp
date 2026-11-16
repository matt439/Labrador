#include "tools/save_check/recovery_io.h"

#include "engine/core/file_path.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(__EMSCRIPTEN__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#else
#include <Windows.h>
#endif

namespace save_example::detail
{
	namespace
	{
		std::runtime_error failure(const std::filesystem::path& path, const char* step,
			unsigned long code)
		{
			return std::runtime_error(labrador::path_to_utf8(path) + ": " + step +
				" failed (system error " + std::to_string(code) + ").");
		}
	}

	void write_new_file(const std::filesystem::path& path, std::string_view bytes)
	{
#if defined(__EMSCRIPTEN__)
		const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (file < 0) throw failure(path, "exclusive staged open", static_cast<unsigned long>(errno));
		try
		{
			std::size_t written = 0;
			while (written != bytes.size())
			{
				const ssize_t count = ::write(file, bytes.data() + written, bytes.size() - written);
				if (count <= 0) throw failure(path, "staged write", static_cast<unsigned long>(errno));
				written += static_cast<std::size_t>(count);
			}
		}
		catch (...)
		{
			::close(file);
			throw;
		}
		if (::close(file) != 0) throw failure(path, "staged close", static_cast<unsigned long>(errno));
#else
		const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) throw failure(path, "exclusive staged open", GetLastError());
		try
		{
			std::size_t written = 0;
			while (written != bytes.size())
			{
				const DWORD count = static_cast<DWORD>(std::min(bytes.size() - written,
					static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
				DWORD completed = 0;
				if (!WriteFile(file, bytes.data() + written, count, &completed, nullptr) || completed == 0)
				{
					throw failure(path, "staged write", GetLastError());
				}
				written += completed;
			}
			if (!FlushFileBuffers(file)) throw failure(path, "staged flush", GetLastError());
		}
		catch (...)
		{
			CloseHandle(file);
			throw;
		}
		if (!CloseHandle(file)) throw failure(path, "staged close", GetLastError());
#endif
	}

	void publish_file(const std::filesystem::path& draft,
		const std::filesystem::path& inactive)
	{
#if defined(__EMSCRIPTEN__)
		std::filesystem::rename(draft, inactive);
#else
		// An interrupted validated draft can be retried by a new process.
		// Flush that existing file as well as drafts written in this session.
		const HANDLE file = CreateFileW(draft.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) throw failure(draft, "publication flush open", GetLastError());
		if (!FlushFileBuffers(file))
		{
			const DWORD error = GetLastError();
			CloseHandle(file);
			throw failure(draft, "publication flush", error);
		}
		if (!CloseHandle(file)) throw failure(draft, "publication flush close", GetLastError());
		const bool existing = std::filesystem::exists(inactive);
		const BOOL replaced = existing
			? ReplaceFileW(inactive.c_str(), draft.c_str(), nullptr, 0, nullptr, nullptr)
			: MoveFileExW(draft.c_str(), inactive.c_str(), 0);
		if (!replaced) throw failure(inactive, "inactive generation publication", GetLastError());
#endif
	}
}

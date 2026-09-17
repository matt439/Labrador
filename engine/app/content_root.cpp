#include "engine/app/content_root.h"

#include <Windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace labrador
{
	std::string executable_directory()
	{
		// Wide, then narrowed by the filesystem library, because the narrow
		// GetModuleFileNameA answers in the ANSI code page and a path with a
		// character outside it comes back with question marks in it. The
		// buffer grows until the answer fits: the documented failure is a
		// truncated path with the buffer's size as the return value, which is
		// the one outcome a caller cannot use.
		std::vector<wchar_t> buffer(MAX_PATH);
		for (;;)
		{
			const DWORD written = GetModuleFileNameW(nullptr, buffer.data(),
				static_cast<DWORD>(buffer.size()));
			if (written == 0)
			{
				throw std::runtime_error(
					"executable_directory - GetModuleFileNameW failed.");
			}
			if (written < buffer.size())
			{
				break;
			}
			buffer.resize(buffer.size() * 2);
		}

		const std::filesystem::path executable(buffer.data());
		std::string directory = executable.parent_path().string();
		if (directory.empty() || (directory.back() != '\\' &&
			directory.back() != '/'))
		{
			directory += '\\';
		}
		return directory;
	}

	std::string resolved_under(const std::string& directory,
		const std::string& path)
	{
		if (std::filesystem::path(path).is_absolute())
		{
			return path;
		}

		std::string resolved = directory;
		if (!resolved.empty() && resolved.back() != '\\' &&
			resolved.back() != '/')
		{
			resolved += '\\';
		}
		resolved += path;
		return resolved;
	}

	AssetManifest anchored_to_source(AssetManifest manifest)
	{
		if (manifest.source_path.empty())
		{
			return manifest;
		}

		// The manifest's own folder, from the path it was read at - which is
		// already resolved, so this is absolute whenever that was.
		const std::string base = std::filesystem::path(manifest.source_path)
			.parent_path().string();

		for (AssetEntry& entry : manifest.entries)
		{
			entry.directory = resolved_under(base, entry.directory);
		}
		return manifest;
	}
}

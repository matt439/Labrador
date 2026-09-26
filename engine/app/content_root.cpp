#include "engine/app/content_root.h"
#include "engine/core/file_path.h"

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
		// Keep the native path wide until explicitly encoding it as UTF-8.
		// Grow the buffer until GetModuleFileNameW returns an untruncated path.
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
		std::string directory = path_to_utf8(executable.parent_path());
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
		if (path_from_utf8(path).is_absolute())
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
		const std::string base = path_to_utf8(
			path_from_utf8(manifest.source_path).parent_path());

		for (AssetEntry& entry : manifest.entries)
		{
			entry.directory = resolved_under(base, entry.directory);
		}
		return manifest;
	}
}

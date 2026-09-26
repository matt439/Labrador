#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace labrador
{
	// Asset paths cross engine APIs as UTF-8. Convert only at the filesystem
	// boundary, so Windows' active ANSI code page cannot replace characters.
	inline std::filesystem::path path_from_utf8(std::string_view path)
	{
		return std::filesystem::path(std::u8string(path.begin(), path.end()));
	}

	inline std::string path_to_utf8(const std::filesystem::path& path)
	{
		const std::u8string encoded = path.u8string();
		return std::string(reinterpret_cast<const char*>(encoded.data()),
			encoded.size());
	}
}

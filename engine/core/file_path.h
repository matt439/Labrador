#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace labrador
{
	// Engine content paths are UTF-8, independent of the process ANSI code
	// page. Convert at every filesystem/API boundary. Embedded NULs are
	// rejected rather than opening a truncated path.
	std::filesystem::path path_from_utf8(std::string_view path);
	std::string path_to_utf8(const std::filesystem::path& path);
}

#include "engine/core/file_path.h"

#include <stdexcept>

namespace labrador
{
	std::filesystem::path path_from_utf8(std::string_view path)
	{
		if (path.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument("A content path contains an embedded NUL.");
		}
		return std::filesystem::path(std::u8string(path.begin(), path.end()));
	}

	std::string path_to_utf8(const std::filesystem::path& path)
	{
		const std::u8string bytes = path.u8string();
		if (bytes.find(u8'\0') != std::u8string::npos)
		{
			throw std::invalid_argument("A content path contains an embedded NUL.");
		}
		return std::string(bytes.begin(), bytes.end());
	}
}

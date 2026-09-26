#include <doctest/doctest.h>

#include "engine/core/file_path.h"

#include <stdexcept>
#include <string>

TEST_CASE("UTF-8 paths round trip through native wide paths")
{
	const std::filesystem::path native(L"C:\\content-\u6f22-\U0001f680\\asset.dds");
	const std::string utf8 = labrador::path_to_utf8(native);
	CHECK(labrador::path_from_utf8(utf8) == native);
	CHECK(utf8.find('\0') == std::string::npos);
}

TEST_CASE("content paths reject embedded NUL rather than opening a prefix")
{
	CHECK_THROWS_AS(labrador::path_from_utf8(std::string("asset\0other", 11)),
		std::invalid_argument);
}

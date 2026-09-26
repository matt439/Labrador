#include <doctest/doctest.h>

#include "engine/core/byte_reader.h"
#include "engine/core/file_path.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

TEST_CASE("UTF-8 paths preserve characters outside the system code page")
{
	const std::string encoded = "\xE6\xB5\x8B\xE8\xAF\x95_\xF0\x9F\x8E\xA8";
	const std::filesystem::path native(L"\u6D4B\u8BD5_\U0001F3A8");
	CHECK(labrador::path_from_utf8(encoded) == native);
	CHECK(labrador::path_to_utf8(native) == encoded);
	CHECK(labrador::path_to_utf8(labrador::path_from_utf8(encoded)) == encoded);
}

TEST_CASE("binary assets can be read from a Unicode directory and filename")
{
	const std::filesystem::path folder = std::filesystem::temp_directory_path() /
		("labrador_bytes_" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count())) /
		L"\u6D4B\u8BD5_\U0001F3A8";
	std::filesystem::create_directories(folder);
	struct Cleanup
	{
		std::filesystem::path folder;
		~Cleanup()
		{
			std::error_code error;
			std::filesystem::remove_all(this->folder, error);
		}
	} cleanup{ folder.parent_path() };
	const std::filesystem::path file = folder / L"\u7D20\u6750.bin";
	{
		std::ofstream output(file, std::ios::binary);
		output.write("\0\x7f\xff", 3);
		REQUIRE(output.good());
	}
	const std::vector<unsigned char> expected{ 0, 127, 255 };
	CHECK(labrador::read_file_bytes(labrador::path_to_utf8(file)) == expected);
}

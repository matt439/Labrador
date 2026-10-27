#pragma once

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace labrador_tests
{
	// A file in the temporary directory, removed when the test leaves. The
	// readers under test take a path, so testing them means writing files.
	//
	// `name` ends the file's name, so a failure message reads as the case
	// that wrote it; a number before it gives every file in the run a name
	// of its own, so a leftover from a crashed run is never read as this
	// run's input.
	class ScratchFile
	{
	public:
		ScratchFile(const std::string& name, std::string_view contents)
		{
			this->write(name, contents.data(), contents.size());
		}

		ScratchFile(const std::string& name,
			const std::vector<unsigned char>& bytes)
		{
			this->write(name, reinterpret_cast<const char*>(bytes.data()),
				bytes.size());
		}

		~ScratchFile()
		{
			std::error_code ignored;
			std::filesystem::remove(this->path_, ignored);
		}

		ScratchFile(const ScratchFile&) = delete;
		ScratchFile& operator=(const ScratchFile&) = delete;

		const std::string& path() const { return this->path_; }

	private:
		void write(const std::string& name, const char* data, size_t size)
		{
			this->path_ = (std::filesystem::temp_directory_path() /
				("labrador_" + std::to_string(next_id()) + "_" + name)).string();

			std::ofstream file(this->path_, std::ios::binary);
			REQUIRE(file.good());
			if (size != 0)
			{
				file.write(data, static_cast<std::streamsize>(size));
			}
			REQUIRE(file.good());
		}

		static int next_id()
		{
			static int id = 0;
			return ++id;
		}

		std::string path_;
	};
}

#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace labrador_tests
{
	// A little-endian DWORD written over `bytes` at `at`, for a test that
	// edits a real file's header in place or builds one from nothing.
	inline void put_u32_le(std::vector<unsigned char>& bytes, size_t at,
		uint32_t value)
	{
		REQUIRE(at + 4 <= bytes.size());
		for (size_t i = 0; i < 4; i++)
		{
			bytes[at + i] =
				static_cast<unsigned char>((value >> (8 * i)) & 0xFFu);
		}
	}

	// A number of its own for every scratch file and folder in the run.
	inline int next_scratch_id()
	{
		static int id = 0;
		return ++id;
	}

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
				("labrador_" + std::to_string(next_scratch_id()) + "_" + name)).string();

			std::ofstream file(this->path_, std::ios::binary);
			REQUIRE(file.good());
			if (size != 0)
			{
				file.write(data, static_cast<std::streamsize>(size));
			}
			REQUIRE(file.good());
		}

		std::string path_;
	};

	// A folder in the temporary directory, removed with everything in it when
	// the test leaves - including by a failed REQUIRE - and named as
	// ScratchFile names a file, so a leftover from a crashed run is never
	// read as this run's content.
	class ScratchFolder
	{
	public:
		explicit ScratchFolder(const std::filesystem::path& name)
		{
			std::filesystem::path leaf(
				"labrador_" + std::to_string(next_scratch_id()) + "_");
			leaf += name;
			this->path_ = std::filesystem::temp_directory_path() / leaf;
			std::filesystem::create_directories(this->path_);
		}

		~ScratchFolder()
		{
			std::error_code ignored;
			std::filesystem::remove_all(this->path_, ignored);
		}

		ScratchFolder(const ScratchFolder&) = delete;
		ScratchFolder& operator=(const ScratchFolder&) = delete;

		const std::filesystem::path& path() const { return this->path_; }

	private:
		std::filesystem::path path_;
	};
}

#include "engine/core/byte_reader.h"
#include "engine/core/file_path.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace labrador
{
	ByteReader::ByteReader(const std::vector<unsigned char>& bytes,
		const std::string& what)
		: bytes_(bytes), what_(what)
	{

	}

	uint32_t ByteReader::read_u32()
	{
		this->require(4);
		const size_t at = this->at_;
		this->at_ += 4;
		return static_cast<uint32_t>(this->bytes_[at]) |
			(static_cast<uint32_t>(this->bytes_[at + 1]) << 8) |
			(static_cast<uint32_t>(this->bytes_[at + 2]) << 16) |
			(static_cast<uint32_t>(this->bytes_[at + 3]) << 24);
	}

	int32_t ByteReader::read_i32()
	{
		return static_cast<int32_t>(this->read_u32());
	}

	// Through the bit pattern, because that is what a file holds: a
	// little-endian IEEE-754 single. Reading it as an integer and copying the
	// bits is the only spelling that does not depend on a struct layout.
	float ByteReader::read_f32()
	{
		const uint32_t bits = this->read_u32();
		float value = 0.0f;
		static_assert(sizeof(value) == sizeof(bits),
			"These files store 32-bit floats.");
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	std::vector<unsigned char> ByteReader::read_bytes(size_t count)
	{
		this->require(count);
		const std::vector<unsigned char>::const_iterator first =
			this->bytes_.begin() + static_cast<std::ptrdiff_t>(this->at_);
		this->at_ += count;
		return std::vector<unsigned char>(first,
			first + static_cast<std::ptrdiff_t>(count));
	}

	void ByteReader::skip(size_t count)
	{
		this->require(count);
		this->at_ += count;
	}

	void ByteReader::require(size_t count) const
	{
		if (count > this->bytes_.size() - this->at_)
		{
			throw std::runtime_error(this->what_ + " ends after " +
				std::to_string(this->bytes_.size()) + " bytes, part way "
				"through a record: " + std::to_string(count) + " more were "
				"wanted at " + std::to_string(this->at_) + ".");
		}
	}

	std::vector<unsigned char> read_file_bytes(const std::string& path)
	{
		const std::filesystem::path native = path_from_utf8(path);

		// Before opening, because some C libraries open a directory for
		// reading and only fail at the first read.
		std::error_code status_error;
		if (std::filesystem::is_directory(native, status_error))
		{
			throw std::runtime_error("Could not read " + path +
				": it is a directory.");
		}

		std::ifstream file(native, std::ios::binary | std::ios::ate);
		if (!file)
		{
			// The open says only that it failed. Asking afterwards keeps the
			// common case one call, and separates a missing name from a file
			// that is there and refused - locked, or not ours to read.
			std::error_code exists_error;
			if (std::filesystem::exists(native, exists_error))
			{
				throw std::runtime_error("Could not open " + path + ".");
			}
			throw std::out_of_range("No file at " + path + ".");
		}

		const std::streamoff size = file.tellg();
		if (size < 0)
		{
			throw std::runtime_error("Could not measure " + path + ".");
		}
		if (static_cast<std::uintmax_t>(size) >
			static_cast<std::uintmax_t>((std::numeric_limits<size_t>::max)()))
		{
			throw std::runtime_error(path + " holds " + std::to_string(size) +
				" bytes, more than this build can address.");
		}

		std::vector<unsigned char> bytes(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		if (!bytes.empty())
		{
			file.read(reinterpret_cast<char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
			if (!file)
			{
				throw std::runtime_error("Could not read " + path + ".");
			}
		}
		return bytes;
	}
}

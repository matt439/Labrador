#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace labrador
{
	// A cursor over a buffer of bytes that cannot walk off the end.
	//
	// EVERY READ IS CHECKED, which the obvious alternative - map a packed struct
	// over the buffer - is not. A truncated file is a plausible thing to have on
	// disk (an interrupted copy, a half-written build step), and the difference
	// between the two designs is a named throw and a read past the end of a heap
	// allocation. It is also the difference between a struct layout this
	// compiler happens to produce and the layout the file actually has.
	//
	// LITTLE-ENDIAN, SPELT OUT. Both file formats this engine reads are, and
	// assembling the value a byte at a time says so - where a memcpy of four
	// bytes would only say "whatever this machine is".
	//
	// IN core/ RATHER THAN render/, which is where both of its callers are,
	// because nothing about it is a picture: it reads numbers out of a file. The
	// second caller is what moved it here; the first one had a copy of it in an
	// anonymous namespace.
	class ByteReader
	{
	public:
		// `bytes` is borrowed and must outlive the reader. `what` names the
		// source in every throw - a path, so the message says which file.
		ByteReader(const std::vector<unsigned char>& bytes,
			const std::string& what);

		uint32_t read_u32();
		int32_t read_i32();
		float read_f32();

		std::vector<unsigned char> read_bytes(size_t count);
		void skip(size_t count);

		// Throws std::runtime_error naming the source if fewer than `count`
		// bytes are left. Called by every read above, and callable directly by a
		// reader that is about to size a container from a number it has just
		// read - which is the one place a corrupt file can ask for gigabytes.
		void require(size_t count) const;

		size_t position() const { return this->at_; }
		size_t remaining() const { return this->bytes_.size() - this->at_; }

	private:
		const std::vector<unsigned char>& bytes_;
		std::string what_;
		size_t at_ = 0;
	};

	// The whole file at `path`, as bytes.
	//
	// Throws std::out_of_range naming the path when there is no file there, and
	// std::runtime_error when there is one and it cannot be read. Those are
	// different facts: the first is a name nobody produced, which is a content
	// bug, and the second is a disk.
	std::vector<unsigned char> read_file_bytes(const std::string& path);
}

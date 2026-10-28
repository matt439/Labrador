#include <doctest/doctest.h>

#include "engine/core/byte_reader.h"

#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using labrador::ByteReader;

TEST_SUITE("ByteReaderTests")
{
	TEST_CASE("unsigned words are decoded in little-endian order")
	{
		const std::vector<unsigned char> bytes = {
			0x78, 0x56, 0x34, 0x12,
			0xef, 0xcd, 0xab, 0x89,
			0x00, 0x00, 0x00, 0x00,
			0xff, 0xff, 0xff, 0xff
		};
		ByteReader reader(bytes, "unsigned-words.bin");

		CHECK(reader.read_u32() == 0x12345678u);
		CHECK(reader.read_u32() == 0x89abcdefu);
		CHECK(reader.read_u32() == 0u);
		CHECK(reader.read_u32() == 0xffffffffu);
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("signed words preserve their stored two's-complement values")
	{
		const std::vector<unsigned char> bytes = {
			0x00, 0x00, 0x00, 0x80,
			0xff, 0xff, 0xff, 0xff,
			0xfe, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0x7f
		};
		ByteReader reader(bytes, "signed-words.bin");

		CHECK(reader.read_i32() == (std::numeric_limits<int32_t>::min)());
		CHECK(reader.read_i32() == -1);
		CHECK(reader.read_i32() == -2);
		CHECK(reader.read_i32() == (std::numeric_limits<int32_t>::max)());
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("float reads preserve little-endian IEEE-754 bit patterns")
	{
		const std::vector<unsigned char> bytes = {
			0x00, 0x00, 0x80, 0x3f,
			0x00, 0x00, 0x20, 0xc0,
			0x00, 0x00, 0x00, 0x80,
			0x00, 0x00, 0x80, 0x7f,
			0x45, 0x23, 0xc1, 0x7f
		};
		ByteReader reader(bytes, "float-words.bin");

		CHECK(std::bit_cast<uint32_t>(reader.read_f32()) == 0x3f800000u);
		CHECK(std::bit_cast<uint32_t>(reader.read_f32()) == 0xc0200000u);
		CHECK(std::bit_cast<uint32_t>(reader.read_f32()) == 0x80000000u);
		CHECK(std::bit_cast<uint32_t>(reader.read_f32()) == 0x7f800000u);
		CHECK(std::bit_cast<uint32_t>(reader.read_f32()) == 0x7fc12345u);
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("mixed reads and skips advance one shared cursor")
	{
		const std::vector<unsigned char> bytes = {
			0xa1, 0xb2, 0xc3, 0xd4, 0x55, 0x66,
			0x10, 0x20, 0x30, 0xff, 0xff, 0xff, 0xff, 0x77
		};
		const std::vector<unsigned char> expected_chunk = { 0x10, 0x20, 0x30 };
		const std::vector<unsigned char> expected_tail = { 0x77 };
		ByteReader reader(bytes, "mixed-record.bin");

		CHECK(reader.position() == 0);
		CHECK(reader.remaining() == bytes.size());
		CHECK(reader.read_u32() == 0xd4c3b2a1u);
		CHECK(reader.position() == 4);
		CHECK(reader.remaining() == 10);
		reader.skip(2);
		CHECK(reader.position() == 6);
		CHECK(reader.remaining() == 8);
		CHECK(reader.read_bytes(3) == expected_chunk);
		CHECK(reader.position() == 9);
		CHECK(reader.remaining() == 5);
		CHECK(reader.read_i32() == -1);
		CHECK(reader.read_bytes(1) == expected_tail);
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("requiring available bytes only checks the boundary")
	{
		const std::vector<unsigned char> bytes = { 0x78, 0x56, 0x34, 0x12 };
		ByteReader reader(bytes, "checked-record.bin");

		CHECK_NOTHROW(reader.require(0));
		CHECK_NOTHROW(reader.require(bytes.size()));
		CHECK(reader.position() == 0);
		CHECK(reader.remaining() == bytes.size());
		CHECK(reader.read_u32() == 0x12345678u);
		CHECK_NOTHROW(reader.require(0));
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("truncated operations leave the cursor available for a valid read")
	{
		const std::vector<unsigned char> bytes = { 0x55, 0x10, 0x20, 0x30 };
		const std::vector<unsigned char> expected = { 0x10, 0x20, 0x30 };
		ByteReader reader(bytes, "truncated-record.bin");
		reader.skip(1);

		SUBCASE("unsigned word")
		{
			CHECK_THROWS_AS(reader.read_u32(), std::runtime_error);
		}
		SUBCASE("signed word")
		{
			CHECK_THROWS_AS(reader.read_i32(), std::runtime_error);
		}
		SUBCASE("float word")
		{
			CHECK_THROWS_AS(reader.read_f32(), std::runtime_error);
		}
		SUBCASE("byte chunk")
		{
			CHECK_THROWS_AS(reader.read_bytes(4), std::runtime_error);
		}
		SUBCASE("skip")
		{
			CHECK_THROWS_AS(reader.skip(4), std::runtime_error);
		}
		SUBCASE("direct boundary check")
		{
			CHECK_THROWS_AS(reader.require(4), std::runtime_error);
		}

		CHECK(reader.position() == 1);
		CHECK(reader.remaining() == 3);
		CHECK(reader.read_bytes(3) == expected);
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("zero-byte operations succeed on an empty buffer")
	{
		const std::vector<unsigned char> bytes;
		ByteReader reader(bytes, "empty-record.bin");

		CHECK_NOTHROW(reader.require(0));
		CHECK_NOTHROW(reader.skip(0));
		CHECK(reader.read_bytes(0).empty());
		CHECK_THROWS_AS(reader.read_u32(), std::runtime_error);
		CHECK_THROWS_AS(reader.read_bytes(1), std::runtime_error);
		CHECK_THROWS_AS(reader.skip(1), std::runtime_error);
		CHECK(reader.position() == 0);
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("zero-byte operations succeed at the end of a nonempty buffer")
	{
		const std::vector<unsigned char> bytes = { 0x55, 0x66 };
		ByteReader reader(bytes, "finished-record.bin");
		reader.skip(bytes.size());

		CHECK_NOTHROW(reader.require(0));
		CHECK_NOTHROW(reader.skip(0));
		CHECK(reader.read_bytes(0).empty());
		CHECK_THROWS_AS(reader.read_u32(), std::runtime_error);
		CHECK_THROWS_AS(reader.read_bytes(1), std::runtime_error);
		CHECK_THROWS_AS(reader.skip(1), std::runtime_error);
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("maximum-size requests are refused after the cursor has advanced")
	{
		const std::vector<unsigned char> bytes = { 0x55, 0x78, 0x56, 0x34, 0x12 };
		ByteReader reader(bytes, "oversized-record.bin");
		reader.skip(1);
		const size_t maximum_count = (std::numeric_limits<size_t>::max)();

		CHECK_THROWS_AS(reader.require(maximum_count), std::runtime_error);
		CHECK_THROWS_AS(reader.skip(maximum_count), std::runtime_error);
		CHECK(reader.position() == 1);
		CHECK(reader.remaining() == 4);
		CHECK(reader.read_u32() == 0x12345678u);
		CHECK(reader.position() == bytes.size());
		CHECK(reader.remaining() == 0);
	}

	TEST_CASE("read byte chunks own their storage after the source is gone")
	{
		std::vector<unsigned char> copy;
		{
			const std::vector<unsigned char> bytes = { 0x10, 0x20, 0x30, 0x40 };
			ByteReader reader(bytes, "copied-record.bin");
			copy = reader.read_bytes(3);
			REQUIRE(copy.size() == 3);
			copy[0] = 0x99;
			CHECK(bytes[0] == 0x10);
			CHECK(reader.position() == 3);
			CHECK(reader.remaining() == 1);
		}
		const std::vector<unsigned char> expected = { 0x99, 0x20, 0x30 };
		CHECK(copy == expected);
	}

	TEST_CASE("boundary diagnostics identify the source and the failed location")
	{
		const std::vector<unsigned char> bytes = {
			0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90
		};
		const std::string source = "diagnostic-record.bin";
		ByteReader reader(bytes, source);
		reader.skip(2);

		try
		{
			reader.require(8);
			FAIL("an oversized request must throw");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message = error.what();
			CHECK(message.find(source) != std::string::npos);
			CHECK(message.find("9 bytes") != std::string::npos);
			CHECK(message.find("8 more") != std::string::npos);
			CHECK(message.find("at 2") != std::string::npos);
		}
		CHECK(reader.position() == 2);
		CHECK(reader.remaining() == 7);
	}
}

#include <doctest/doctest.h>

#include "engine/app/win32/keyboard_input.h"
#include "engine/input/keyboard.h"

#include <Windows.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <set>

using namespace labrador;

namespace
{
	constexpr char16_t first_high = 0xd800;
	constexpr char16_t last_high = 0xdbff;
	constexpr char16_t first_low = 0xdc00;
	constexpr char16_t last_low = 0xdfff;

	intptr_t key_position(unsigned int scan, bool extended = false)
	{
		return static_cast<intptr_t>((static_cast<uint64_t>(scan) << 16) |
			(extended ? (uint64_t(1) << 24) : 0) | 1u);
	}

	void check_codepoint(Utf16Input& input, char16_t unit, char32_t expected)
	{
		const std::optional<char32_t> actual = input.append(unit);
		REQUIRE(actual.has_value());
		CHECK(static_cast<uint32_t>(*actual) == static_cast<uint32_t>(expected));
	}
}

TEST_CASE("Win32 key translation uses physical positions and extended aliases")
{
	CHECK(key_from_message('Z', key_position(0x11)) == Key::w);
	CHECK(key_from_message('Q', key_position(0x1e)) == Key::a);
	CHECK(key_from_message('Y', key_position(0x2c)) == Key::z);

	struct Alias { unsigned int scan; Key ordinary; Key extended; };
	const Alias aliases[] = {
		{ 0x1c, Key::enter, Key::enter },
		{ 0x1d, Key::control, Key::control },
		{ 0x35, Key::slash, Key::numpad_divide },
		{ 0x37, Key::numpad_multiply, Key::print_screen },
		{ 0x38, Key::alt, Key::alt },
		{ 0x47, Key::numpad_7, Key::home },
		{ 0x53, Key::numpad_decimal, Key::del },
	};
	for (const Alias& alias : aliases)
	{
		CAPTURE(alias.scan);
		CHECK(key_from_message(0, key_position(alias.scan)) == alias.ordinary);
		CHECK(key_from_message(0, key_position(alias.scan, true)) == alias.extended);
	}
	CHECK(key_from_message(VK_SHIFT, key_position(0x2a)) == Key::shift);
	CHECK(key_from_message(VK_SHIFT, key_position(0x36)) == Key::shift);
}

TEST_CASE("Win32 Pause and NumLock override scans using the full virtual key")
{
	const intptr_t contradictory = key_position(0xff, true);
	CHECK(key_from_message(VK_PAUSE, contradictory) == Key::pause);
	CHECK(key_from_message(VK_NUMLOCK, contradictory) == Key::num_lock);
	CHECK(key_from_message(0, key_position(0x45)) == Key::num_lock);

	const uint64_t high_bits = uint64_t(1) << 32;
	CHECK(key_from_message(high_bits | VK_PAUSE, key_position(0x11)) == Key::w);
	CHECK(key_from_message(high_bits | VK_NUMLOCK, key_position(0x47, true)) == Key::home);
}

TEST_CASE("Win32 key translation ignores message flags and rejects unknown positions")
{
	// Repeat count, reserved/context/transition bits and the signed high bit
	// must not become part of the eight-bit scan position.
	const intptr_t flags = std::numeric_limits<intptr_t>::min() |
		static_cast<intptr_t>(0xfe00ffffu);
	CHECK(key_from_message('Z', key_position(0x11) | flags) == Key::w);
	CHECK(key_from_message(VK_HOME, key_position(0x47, true) | flags) == Key::home);
	CHECK(key_from_message('W', key_position(0x00)) == Key::none);
	CHECK(key_from_message('W', key_position(0xff)) == Key::none);
	CHECK(key_from_message('W', key_position(0x11, true)) == Key::none);
	CHECK(key_from_message(VK_ADD, key_position(0x4e, true)) == Key::none);
}

TEST_CASE("every named key remains reachable through Win32 translation")
{
	std::set<Key> reachable;
	for (unsigned int scan = 0; scan < 256; ++scan)
	{
		reachable.insert(key_from_message(0, key_position(scan)));
		reachable.insert(key_from_message(0, key_position(scan, true)));
	}
	reachable.insert(key_from_message(VK_PAUSE, 0));
	reachable.insert(key_from_message(VK_NUMLOCK, 0));
	for (unsigned int value = 1; value < static_cast<unsigned int>(Key::count); ++value)
	{
		CAPTURE(value);
		CHECK(reachable.contains(static_cast<Key>(value)));
	}
}

TEST_CASE("Win32 UTF-16 assembly passes BMP units and controls through unchanged")
{
	Utf16Input input;
	const char16_t units[] = { 0, 0x1f, 0x7f, u'A', 0xd7ff, 0xe000, 0xfffd, 0xffff };
	for (const char16_t unit : units)
	{
		CAPTURE(static_cast<uint32_t>(unit));
		check_codepoint(input, unit, static_cast<char32_t>(unit));
	}
}

TEST_CASE("Win32 UTF-16 assembly combines boundary pairs and clears the pending high")
{
	struct Pair { char16_t high; char16_t low; char32_t expected; };
	const Pair pairs[] = {
		{ first_high, first_low, 0x10000u },
		{ 0xd83d, 0xde00, 0x1f600u },
		{ last_high, last_low, 0x10ffffu },
	};
	for (const Pair& pair : pairs)
	{
		Utf16Input input;
		CAPTURE(static_cast<uint32_t>(pair.expected));
		CHECK_FALSE(input.append(pair.high).has_value());
		check_codepoint(input, pair.low, pair.expected);
		check_codepoint(input, pair.low, static_cast<char32_t>(pair.low));
	}
}

TEST_CASE("Win32 UTF-16 assembly preserves malformed sequence handling")
{
	Utf16Input input;
	SUBCASE("a new high silently replaces an unmatched high")
	{
		CHECK_FALSE(input.append(first_high).has_value());
		CHECK_FALSE(input.append(last_high).has_value());
		check_codepoint(input, last_low, 0x10ffffu);
	}
	SUBCASE("a BMP character discards an unmatched high without replacement")
	{
		CHECK_FALSE(input.append(first_high).has_value());
		check_codepoint(input, u'A', U'A');
		check_codepoint(input, first_low, static_cast<char32_t>(first_low));
	}
	SUBCASE("NUL is an emitted value and also discards an unmatched high")
	{
		CHECK_FALSE(input.append(first_high).has_value());
		check_codepoint(input, 0, 0);
		check_codepoint(input, first_low, static_cast<char32_t>(first_low));
	}
	SUBCASE("unpaired lows pass through rather than being sanitized")
	{
		check_codepoint(input, first_low, static_cast<char32_t>(first_low));
		check_codepoint(input, last_low, static_cast<char32_t>(last_low));
	}
}

TEST_CASE("Win32 UTF-16 pending units belong to each input instance")
{
	Utf16Input first;
	Utf16Input second;
	CHECK_FALSE(first.append(first_high).has_value());
	check_codepoint(second, last_low, static_cast<char32_t>(last_low));
	CHECK_FALSE(second.append(last_high).has_value());
	check_codepoint(first, first_low, 0x10000u);
	check_codepoint(second, last_low, 0x10ffffu);
}

TEST_CASE("Keyboard replaces a raw unpaired Win32 low surrogate after translation")
{
	Utf16Input input;
	const std::optional<char32_t> raw = input.append(first_low);
	REQUIRE(raw.has_value());
	CHECK(static_cast<uint32_t>(*raw) == static_cast<uint32_t>(first_low));
	Keyboard keyboard;
	keyboard.on_text(*raw);
	keyboard.poll();
	REQUIRE(keyboard.typed().size() == 3);
	CHECK(static_cast<unsigned char>(keyboard.typed()[0]) == 0xefu);
	CHECK(static_cast<unsigned char>(keyboard.typed()[1]) == 0xbfu);
	CHECK(static_cast<unsigned char>(keyboard.typed()[2]) == 0xbdu);
}

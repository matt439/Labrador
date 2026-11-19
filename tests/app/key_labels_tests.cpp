#include <doctest/doctest.h>

#include "engine/app/win32/key_labels.h"
#include "engine/app/win32/keyboard_input.h"
#include "engine/render/text_encoding.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>

using namespace labrador;

namespace
{
	struct LabelFixture
	{
		unsigned int map_result = 0x51;
		std::u16string text = u"q";
		int text_count = 1;
		std::u16string name = u"Named key";
		int name_count = 9;
		bool terminate_name = true;
		int map_calls = 0;
		int unicode_calls = 0;
		int name_calls = 0;
		unsigned int map_scan = 0;
		unsigned int map_type = 0;
		unsigned int unicode_virtual_key = 0;
		unsigned int unicode_scan = 0;
		unsigned int unicode_flags = 0;
		std::uintptr_t map_layout = 0;
		std::uintptr_t unicode_layout = 0;
		std::int32_t name_bits = 0;
		std::array<unsigned char, 256> state{};

		NativeKeyLabelApi api()
		{
			return { this, map, unicode, key_name };
		}

		static unsigned int map(void* context, unsigned int scan,
			unsigned int type, std::uintptr_t layout)
		{
			LabelFixture& self = *static_cast<LabelFixture*>(context);
			++self.map_calls;
			self.map_scan = scan;
			self.map_type = type;
			self.map_layout = layout;
			return self.map_result;
		}

		static int unicode(void* context, unsigned int virtual_key,
			unsigned int scan, const unsigned char* state, char16_t* output,
			int capacity, unsigned int flags, std::uintptr_t layout)
		{
			LabelFixture& self = *static_cast<LabelFixture*>(context);
			++self.unicode_calls;
			self.unicode_virtual_key = virtual_key;
			self.unicode_scan = scan;
			self.unicode_flags = flags;
			self.unicode_layout = layout;
			std::copy_n(state, self.state.size(), self.state.begin());
			const std::size_t copied = std::min(self.text.size(),
				static_cast<std::size_t>(capacity));
			std::copy_n(self.text.begin(), copied, output);
			return self.text_count;
		}

		static int key_name(void* context, std::int32_t bits,
			char16_t* output, int capacity)
		{
			LabelFixture& self = *static_cast<LabelFixture*>(context);
			++self.name_calls;
			self.name_bits = bits;
			const std::size_t copied = std::min(self.name.size(),
				static_cast<std::size_t>(capacity));
			std::copy_n(self.name.begin(), copied, output);
			if (!self.terminate_name && self.name_count >= 0 && self.name_count < capacity)
			{
				output[self.name_count] = u'x';
			}
			return self.name_count;
		}
	};

	constexpr std::uintptr_t layout_identity =
		static_cast<std::uintptr_t>(0x1234567887654321ull);
}

TEST_CASE("native labels translate the existing physical scan with a neutral nonmutating query")
{
	LabelFixture fixture;
	fixture.map_result = 0x51;
	CHECK(key_label_from_layout(Key::a, layout_identity, fixture.api()) == "q");
	CHECK(fixture.map_calls == 1);
	CHECK(fixture.unicode_calls == 1);
	CHECK(fixture.name_calls == 0);
	CHECK(fixture.map_scan == 0x1e);
	CHECK(fixture.map_type == 3);
	CHECK(fixture.map_layout == layout_identity);
	CHECK(fixture.unicode_virtual_key == 0x51);
	CHECK(fixture.unicode_scan == 0x1e);
	CHECK(fixture.unicode_flags == 4);
	CHECK(fixture.unicode_layout == layout_identity);
	CHECK(std::all_of(fixture.state.begin(), fixture.state.end(),
		[](unsigned char value) { return value == 0; }));
}

TEST_CASE("native label scans preserve number row punctuation and the ISO extra position")
{
	struct Position { Key key; unsigned int scan; };
	const Position positions[] = {
		{ Key::digit_0, 0x0b }, { Key::digit_1, 0x02 },
		{ Key::minus, 0x0c }, { Key::equals, 0x0d },
		{ Key::left_bracket, 0x1a }, { Key::right_bracket, 0x1b },
		{ Key::backslash, 0x2b }, { Key::semicolon, 0x27 },
		{ Key::apostrophe, 0x28 }, { Key::grave, 0x29 },
		{ Key::comma, 0x33 }, { Key::period, 0x34 }, { Key::slash, 0x35 },
		{ Key::international_backslash, 0x56 },
	};
	for (const Position& position : positions)
	{
		CAPTURE(position.scan);
		LabelFixture fixture;
		CHECK(key_label_from_layout(position.key, layout_identity, fixture.api()) == "q");
		CHECK(fixture.map_scan == position.scan);
		CHECK(fixture.unicode_scan == position.scan);
		CHECK(fixture.name_calls == 0);
	}
}

TEST_CASE("native character labels retain one valid Unicode scalar and ignore units beyond its count")
{
	LabelFixture fixture;
	fixture.text = u"\u00e9junk";
	CHECK(key_label_from_layout(Key::a, layout_identity, fixture.api()) == "\xc3\xa9");
	fixture.text = u"\U0001f642junk";
	fixture.text_count = 2;
	CHECK(key_label_from_layout(Key::a, layout_identity, fixture.api()) == "\xf0\x9f\x99\x82");
}

TEST_CASE("dead missing ambiguous malformed and control translations are unavailable without a retry")
{
	struct Translation { std::u16string text; int count; };
	const Translation translations[] = {
		{ u"\u00b4", -1 }, { u"x", 0 }, { u"ab", 2 }, { u"abc", 3 },
		{ std::u16string(1, static_cast<char16_t>(0xd800)), 1 },
		{ std::u16string(1, static_cast<char16_t>(0xdc00)), 1 },
		{ std::u16string{ static_cast<char16_t>(0xd800), u'a' }, 2 },
		{ std::u16string{ static_cast<char16_t>(0xdc00), static_cast<char16_t>(0xd800) }, 2 },
		{ u"\r", 1 }, { std::u16string(1, 0), 1 }, { u"\x7f", 1 },
		{ u"\x85", 1 }, { u"\u2028", 1 }, { u" ", 1 },
		{ u"x", std::numeric_limits<int>::max() },
		{ u"x", std::numeric_limits<int>::min() },
	};
	for (const Translation& translation : translations)
	{
		CAPTURE(translation.count);
		LabelFixture fixture;
		fixture.text = translation.text;
		fixture.text_count = translation.count;
		CHECK(key_label_from_layout(Key::a, layout_identity, fixture.api()).empty());
		CHECK(fixture.map_calls == 1);
		CHECK(fixture.unicode_calls == 1);
		CHECK(fixture.name_calls == 0);
	}
}

TEST_CASE("named native keys choose canonical aggregate and extended aliases without character translation")
{
	struct Position { Key key; unsigned int scan; bool extended; };
	const Position positions[] = {
		{ Key::enter, 0x1c, false }, { Key::shift, 0x2a, false },
		{ Key::control, 0x1d, false }, { Key::alt, 0x38, false },
		{ Key::space, 0x39, false }, { Key::pause, 0x45, false },
		{ Key::num_lock, 0x45, true }, { Key::print_screen, 0x37, true },
		{ Key::home, 0x47, true }, { Key::del, 0x53, true },
		{ Key::numpad_7, 0x47, false }, { Key::numpad_divide, 0x35, true },
	};
	for (const Position& position : positions)
	{
		CAPTURE(position.scan);
		CAPTURE(position.extended);
		LabelFixture fixture;
		CHECK(key_label_from_layout(position.key, layout_identity, fixture.api()) == "Named key");
		CHECK(fixture.name_calls == 1);
		CHECK(fixture.map_calls == 0);
		CHECK(fixture.unicode_calls == 0);
		const unsigned int bits = static_cast<unsigned int>(fixture.name_bits);
		CHECK(((bits >> 16) & 0xffu) == position.scan);
		CHECK(((bits >> 24) & 1u) == static_cast<unsigned int>(position.extended));
		CHECK(((bits >> 25) & 1u) == 1u);
	}
}

TEST_CASE("native label positions round trip through the current message translator")
{
	for (unsigned int value = 1; value < static_cast<unsigned int>(Key::count); ++value)
	{
		CAPTURE(value);
		const Key key = static_cast<Key>(value);
		LabelFixture fixture;
		CHECK_FALSE(key_label_from_layout(key, layout_identity, fixture.api()).empty());
		const std::uint64_t virtual_key = key == Key::pause ? VK_PAUSE :
			(key == Key::num_lock ? VK_NUMLOCK : 0);
		const std::intptr_t bits = fixture.name_calls != 0 ? fixture.name_bits :
			static_cast<std::intptr_t>(fixture.map_scan << 16);
		CHECK(key_from_message(virtual_key, bits) == key);
	}
}

TEST_CASE("native named labels validate UTF16 controls terminators and clipping")
{
	LabelFixture fixture;
	fixture.name = u"Cl\u00e9";
	fixture.name_count = 3;
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()) == "Cl\xc3\xa9");
	fixture.name = u"ab\n";
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
	fixture.name = u"   ";
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
	fixture.name = std::u16string(1, static_cast<char16_t>(0xdfff));
	fixture.name_count = 1;
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
	fixture.name = u"Enter";
	fixture.name_count = 0;
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
	fixture.name_count = 5;
	fixture.terminate_name = false;
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
	fixture.name_count = 127;
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
	fixture.name_count = 128;
	CHECK(key_label_from_layout(Key::enter, layout_identity, fixture.api()).empty());
}

TEST_CASE("unknown native keys null layouts and missing callbacks never query unavailable services")
{
	LabelFixture fixture;
	for (const Key key : { Key::none, Key::count, static_cast<Key>(255) })
	{
		CHECK(key_label_from_layout(key, layout_identity, fixture.api()).empty());
	}
	CHECK(key_label_from_layout(Key::a, 0, fixture.api()).empty());
	CHECK(key_label_from_layout(Key::enter, 0, fixture.api()).empty());
	CHECK(fixture.map_calls == 0);
	CHECK(fixture.unicode_calls == 0);
	CHECK(fixture.name_calls == 0);
	CHECK(key_label_from_layout(Key::a, layout_identity, NativeKeyLabelApi()).empty());
	CHECK(key_label_from_layout(Key::enter, layout_identity, NativeKeyLabelApi()).empty());
	NativeKeyLabelApi missing_unicode = fixture.api();
	missing_unicode.to_unicode = nullptr;
	CHECK(key_label_from_layout(Key::a, layout_identity, missing_unicode).empty());
	CHECK(fixture.map_calls == 0);
	fixture.map_result = 0;
	CHECK(key_label_from_layout(Key::a, layout_identity, fixture.api()).empty());
	CHECK(fixture.map_calls == 1);
	CHECK(fixture.unicode_calls == 0);
	CHECK(fixture.name_calls == 0);
}

TEST_CASE("native labels can read the current layout without changing keyboard or layout state")
{
	const HKL layout = GetKeyboardLayout(0);
	REQUIRE(layout != nullptr);
	std::array<unsigned char, 256> before{};
	std::array<unsigned char, 256> after{};
	REQUIRE(GetKeyboardState(before.data()));
	const std::uintptr_t identity = reinterpret_cast<std::uintptr_t>(layout);
	CHECK_FALSE(key_label_from_layout(Key::enter, identity).empty());
	const std::string character = key_label_from_layout(Key::a, identity);
	if (!character.empty())
	{
		CHECK(narrow(widen(character)) == character);
	}
	CHECK(GetKeyboardLayout(0) == layout);
	REQUIRE(GetKeyboardState(after.data()));
	CHECK(before == after);
}

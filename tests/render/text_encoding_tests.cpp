#include <doctest/doctest.h>

#include "engine/render/text_encoding.h"

#include <string>

using labrador::narrow;
using labrador::widen;

TEST_CASE("ASCII crosses unchanged, and nothing crosses as nothing")
{
	CHECK(widen("Press A to proceed") == L"Press A to proceed");
	CHECK(widen("").empty());
}

TEST_CASE("multi-byte UTF-8 becomes the character it encodes")
{
	// The curly apostrophe a text editor inserts on its own, which is where
	// the whole missing-glyph path starts: three bytes narrow, one unit wide,
	// and outside the 95 characters every font in this tree carries.
	CHECK(widen("don\xE2\x80\x99t") == L"don\u2019t");

	// Past the basic plane: one character, two UTF-16 units. That is the unit
	// RenderResources::first_unrenderable counts in, and the unit the font
	// will draw in, which is why it reports per unit rather than per
	// character.
	CHECK(widen("\xF0\x9F\x8E\xAE").size() == 2);
}

TEST_CASE("invalid UTF-8 becomes U+FFFD rather than nothing")
{
	// The promise this header makes - "text about to be drawn should show
	// mojibake, not vanish". U+FFFD is outside the region MakeSpriteFont writes
	// when nobody chooses one, so without a stand-in it is the replacement
	// character that leads into the throw. It draws as the stand-in glyph the
	// font kind installs at load.
	const std::wstring mojibake = widen("bad\xFF\xFE" "bytes");

	CHECK(mojibake.find(L'\uFFFD') != std::wstring::npos);

	// And the text either side of it survives, which is the half of "not
	// vanish" that matters: one bad byte in a weapon description does not cost
	// the description.
	CHECK(mojibake.find(L"bad") != std::wstring::npos);
	CHECK(mojibake.find(L"bytes") != std::wstring::npos);
}

TEST_CASE("bad sequences are cut where MultiByteToWideChar cuts them")
{
	// One implementation on every platform, and on Windows it has to read
	// text exactly as the API it replaced did - these are that API's answers.
	// A truncated sequence is one U+FFFD however many of its bytes were
	// present, and a byte that can open nothing is one U+FFFD by itself.
	CHECK(widen("a\xE2\x80" "b") == L"a\uFFFD" L"b");
	CHECK(widen("\xFF\xFE") == L"\uFFFD\uFFFD");
	CHECK(widen("\x80") == L"\uFFFD");
	CHECK(widen("end\xF0\x9F\x8E") == L"end\uFFFD");
	CHECK(widen("\xC0\xAF") == L"\uFFFD\uFFFD");

	// Overlong forms, encoded surrogates and anything past U+10FFFF are not
	// characters, so none of them decodes to one. Here Windows departs from
	// the Unicode Standard's maximal subparts: the continuation byte the lead
	// refuses goes with the lead, and only the bytes after it stand alone.
	CHECK(widen("\xE0\x80\xAF") == L"\uFFFD\uFFFD");
	CHECK(widen("\xED\xA0\x80") == L"\uFFFD\uFFFD");
	CHECK(widen("\xF4\x90\x80\x80") == L"\uFFFD\uFFFD\uFFFD");
	CHECK(widen("\xF0\x80") == L"\uFFFD");
}

TEST_CASE("past the basic plane is a surrogate pair whatever wchar_t is")
{
	// 32 bits under clang, 16 under MSVC. The render module counts UTF-16
	// units on both, so this is the same two units on both.
	const std::wstring pad = widen("\xF0\x9F\x8E\xAE");
	REQUIRE(pad.size() == 2);
	CHECK(static_cast<unsigned>(pad[0]) == 0xD83Cu);
	CHECK(static_cast<unsigned>(pad[1]) == 0xDFAEu);

	// The largest code point there is, and the boundary either side of the
	// basic plane.
	CHECK(widen("\xF4\x8F\xBF\xBF").size() == 2);
	CHECK(widen("\xEF\xBF\xBF").size() == 1);
	CHECK(widen("\xF0\x90\x80\x80").size() == 2);
}

TEST_CASE("narrow is widen's inverse for well-formed text")
{
	CHECK(narrow(L"Press A to proceed") == "Press A to proceed");
	CHECK(narrow(L"").empty());

	// One, two, three and four UTF-8 bytes, the last from a surrogate pair.
	const std::string every_length = "A\xC3\xA9\xE2\x80\x99\xF0\x9F\x8E\xAE";
	CHECK(narrow(widen(every_length)) == every_length);

	// The largest code point there is, and the boundary either side of the
	// basic plane.
	CHECK(narrow(widen("\xF4\x8F\xBF\xBF")) == "\xF4\x8F\xBF\xBF");
	CHECK(narrow(widen("\xEF\xBF\xBF")) == "\xEF\xBF\xBF");
	CHECK(narrow(widen("\xF0\x90\x80\x80")) == "\xF0\x90\x80\x80");
}

TEST_CASE("an unpaired surrogate narrows to U+FFFD and the rest survives")
{
	std::wstring lone_high = L"a";
	lone_high += static_cast<wchar_t>(0xD83C);
	lone_high += L"b";
	CHECK(narrow(lone_high) == "a\xEF\xBF\xBD" "b");

	std::wstring lone_low;
	lone_low += static_cast<wchar_t>(0xDFAE);
	CHECK(narrow(lone_low) == "\xEF\xBF\xBD");

	// A high surrogate at the very end has no partner to wait for.
	std::wstring trailing_high = L"end";
	trailing_high += static_cast<wchar_t>(0xD83C);
	CHECK(narrow(trailing_high) == "end\xEF\xBF\xBD");

	// The pair reversed is two strays, not one character.
	std::wstring reversed;
	reversed += static_cast<wchar_t>(0xDFAE);
	reversed += static_cast<wchar_t>(0xD83C);
	CHECK(narrow(reversed) == "\xEF\xBF\xBD\xEF\xBF\xBD");
}

TEST_CASE("a 32-bit wchar_t may hold a whole code point")
{
	// Only clang's platforms can say this; under MSVC every unit is 16 bits
	// and the surrogate pair above is the only spelling.
	if constexpr (sizeof(wchar_t) == 4)
	{
		std::wstring pad;
		pad += static_cast<wchar_t>(0x1F3AE);
		CHECK(narrow(pad) == "\xF0\x9F\x8E\xAE");

		std::wstring past_unicode;
		past_unicode += static_cast<wchar_t>(0x110000);
		CHECK(narrow(past_unicode) == "\xEF\xBF\xBD");
	}
}

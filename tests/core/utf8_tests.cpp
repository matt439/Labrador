#include <doctest/doctest.h>

#include "engine/core/utf8.h"

#include <string>

using namespace labrador;

namespace
{
	std::string encoded(char32_t code_point)
	{
		std::string text;
		append_utf8(text, code_point);
		return text;
	}
}

TEST_CASE("UTF-8 encoding uses one to four bytes at each boundary")
{
	CHECK(encoded(U'A') == "A");
	CHECK(encoded(0x7Fu) == "\x7F");
	CHECK(encoded(0x80u) == "\xC2\x80");
	CHECK(encoded(0x7FFu) == "\xDF\xBF");
	CHECK(encoded(0x800u) == "\xE0\xA0\x80");
	CHECK(encoded(0xFFFFu) == "\xEF\xBF\xBF");
	CHECK(encoded(0x10000u) == "\xF0\x90\x80\x80");
	CHECK(encoded(0x10FFFFu) == "\xF4\x8F\xBF\xBF");
}

TEST_CASE("UTF-8 encoding replaces surrogates and values past U+10FFFF")
{
	const std::string replacement = "\xEF\xBF\xBD";
	CHECK(encoded(0xD800u) == replacement);
	CHECK(encoded(0xDFFFu) == replacement);
	CHECK(encoded(0x110000u) == replacement);
	CHECK(encoded(0xD7FFu) == "\xED\x9F\xBF");
	CHECK(encoded(0xE000u) == "\xEE\x80\x80");
}

TEST_CASE("UTF-8 encoding appends to what the text already holds")
{
	std::string text = "x";
	append_utf8(text, 0xE9u);
	append_utf8(text, U'y');
	CHECK(text == "x\xC3\xA9y");
}

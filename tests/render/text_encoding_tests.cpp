#include <doctest/doctest.h>

#include "engine/render/text_encoding.h"

#include <string>

using artattack::widen;

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
	// mojibake, not vanish" - and the one that used to lead into the throw it
	// was written to avoid, because U+FFFD is outside the region
	// MakeSpriteFont writes when nobody chooses one. It draws as the stand-in
	// glyph the font kind now installs at load.
	const std::wstring mojibake = widen("bad\xFF\xFE" "bytes");

	CHECK(mojibake.find(L'\uFFFD') != std::wstring::npos);

	// And the text either side of it survives, which is the half of "not
	// vanish" that matters: one bad byte in a weapon description does not cost
	// the description.
	CHECK(mojibake.find(L"bad") != std::wstring::npos);
	CHECK(mojibake.find(L"bytes") != std::wstring::npos);
}

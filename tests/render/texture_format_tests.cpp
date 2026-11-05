#include <doctest/doctest.h>

#include "engine/render/texture_format.h"

#include <string>

// The names a refusal puts in its message. The backends that refuse a format
// say it with format_name, so a misspelling here would be a misspelling in
// every one of them.

TEST_CASE("format_name spells each format as the enum does")
{
	using labrador::TextureFormat;

	CHECK(labrador::format_name(TextureFormat::r8g8b8a8_unorm) ==
		"r8g8b8a8_unorm");
	CHECK(labrador::format_name(TextureFormat::b8g8r8a8_unorm) ==
		"b8g8r8a8_unorm");
	CHECK(labrador::format_name(TextureFormat::b4g4r4a4_unorm) ==
		"b4g4r4a4_unorm");
	CHECK(labrador::format_name(TextureFormat::bc1_unorm) == "bc1_unorm");
	CHECK(labrador::format_name(TextureFormat::bc2_unorm) == "bc2_unorm");
	CHECK(labrador::format_name(TextureFormat::bc3_unorm) == "bc3_unorm");
}

TEST_CASE("format_name answers a value outside the enum with its number")
{
	// Not another format's name: a refusal that named one would send its
	// reader after the wrong file.
	const labrador::TextureFormat unknown =
		static_cast<labrador::TextureFormat>(99);

	CHECK(labrador::format_name(unknown) == "TextureFormat 99");
}

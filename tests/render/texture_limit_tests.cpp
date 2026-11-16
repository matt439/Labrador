#include <doctest/doctest.h>

#include "engine/render/renderer_checks.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

using labrador::check_texture_dimension_limit;

TEST_CASE("a known texture dimension bound includes both edges")
{
	const std::optional<std::uint32_t> limit = 8192u;
	CHECK_NOTHROW(check_texture_dimension_limit("atlas", 8192, 8192, limit));
	CHECK_NOTHROW(check_texture_dimension_limit("atlas", 1, 8192, limit));
	CHECK_NOTHROW(check_texture_dimension_limit("atlas", 8192, 1, limit));
	CHECK_THROWS_AS(check_texture_dimension_limit("atlas", 8193, 1, limit),
		std::runtime_error);
	CHECK_THROWS_AS(check_texture_dimension_limit("atlas", 1, 8193, limit),
		std::runtime_error);
}

TEST_CASE("an oversized texture diagnostic names the request and its device bound")
{
	const std::string padded_name = "prefix/hero/suffix";
	const std::string_view name(padded_name.data() + 7, 4);
	for (const bool wide : { false, true })
	{
		const int width = wide ? 41 : 9;
		const int height = wide ? 9 : 41;
		try
		{
			check_texture_dimension_limit(name, width, height, 40u);
			FAIL("an oversized request was accepted");
		}
		catch (const std::runtime_error& error)
		{
			const std::string message = error.what();
			CHECK(message.find("hero") != std::string::npos);
			CHECK(message.find("prefix") == std::string::npos);
			CHECK(message.find("suffix") == std::string::npos);
			CHECK(message.find(wide ? "41x9" : "9x41") != std::string::npos);
			CHECK(message.find("40") != std::string::npos);
		}
	}
}

TEST_CASE("an unavailable texture dimension bound adds no guessed maximum")
{
	const int largest = std::numeric_limits<int>::max();
	CHECK_NOTHROW(check_texture_dimension_limit("unmeasured", largest, largest,
		std::nullopt));

	// The API's unsigned bound need not fit the dimensions' signed type.
	CHECK_NOTHROW(check_texture_dimension_limit("wide_limit", largest, largest,
		std::numeric_limits<std::uint32_t>::max()));
}

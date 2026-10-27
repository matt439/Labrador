#include <doctest/doctest.h>

#include "engine/core/random.h"

#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

// THE NUMBERS BELOW ARE THE CONTRACT. Every platform's CI runs this file, so a
// value written here is a value every platform must produce: the same seed
// giving the same game on Windows and in a browser is the reason Random exists.

using labrador::Random;

TEST_CASE("a seed gives the same stream on every platform")
{
	Random from_zero;
	CHECK(from_zero.next() == 0x64625032u);
	CHECK(from_zero.next() == 0xd9c0799cu);
	CHECK(from_zero.next() == 0xaf362e10u);
	CHECK(from_zero.next() == 0x7fa88912u);

	Random seeded(12345u);
	CHECK(seeded.next() == 0xc3b24a19u);
	CHECK(seeded.next() == 0x248b6df5u);
	CHECK(seeded.next() == 0xaf4b7724u);
}

TEST_CASE("the position is the stream: a copy draws what the original would")
{
	Random original(7u);
	original.next();
	original.next();

	Random copy = original;
	Random resumed(original.position());
	CHECK(copy == original);
	CHECK(resumed == original);

	const std::uint32_t expected = original.next();
	CHECK(copy.next() == expected);
	CHECK(resumed.next() == expected);

	// And a stream that has moved on is no longer equal to one that has not.
	CHECK_FALSE(copy == Random(7u));
}

TEST_CASE("uniform_int stays inside its range, both ends included")
{
	Random random(99u);
	std::array<int, 6> seen = {};
	for (int draw = 0; draw < 600; ++draw)
	{
		const int value = random.uniform_int(1, 6);
		REQUIRE(value >= 1);
		REQUIRE(value <= 6);
		++seen[static_cast<size_t>(value - 1)];
	}

	// Six hundred rolls of a fair die miss a face with probability about
	// 6 * (5/6)^600, which is zero for any purpose; a face never seen is a
	// range computed one short.
	for (const int count : seen)
	{
		CHECK(count > 0);
	}
}

TEST_CASE("uniform_int's values are pinned, as next()'s are")
{
	Random random(2026u);
	CHECK(random.uniform_int(0, 9) == 2);
	CHECK(random.uniform_int(-3, 3) == 3);
	CHECK(random.uniform_int(100, 1000) == 960);
}

TEST_CASE("uniform_int with one value draws once and returns it")
{
	Random random(5u);
	Random reference(5u);
	CHECK(random.uniform_int(42, 42) == 42);
	reference.next();
	CHECK(random == reference);
}

TEST_CASE("uniform_int covers the widest range without overflow")
{
	Random random(3u);
	Random reference(3u);
	const int value = random.uniform_int(INT_MIN, INT_MAX);
	const std::int64_t expected = static_cast<std::int64_t>(INT_MIN) +
		static_cast<std::int64_t>(reference.next());
	CHECK(value == static_cast<int>(expected));
}

TEST_CASE("uniform_int refuses a range whose minimum is above its maximum")
{
	Random random;
	CHECK_THROWS_AS(random.uniform_int(2, 1), std::invalid_argument);
	CHECK(random == Random());
}

TEST_CASE("unit() is at least 0 and below 1, in steps of 2^-24")
{
	Random random(11u);
	for (int draw = 0; draw < 1000; ++draw)
	{
		const float value = random.unit();
		REQUIRE(value >= 0.0f);
		REQUIRE(value < 1.0f);
		const float scaled = value * 16777216.0f;
		REQUIRE(scaled == std::floor(scaled));
	}

	// Pinned: the top 24 bits of the first draw from zero, 0x646250, over 2^24.
	CHECK(Random().unit() == static_cast<float>(0x646250) / 16777216.0f);
}

TEST_CASE("uniform_float scales unit() into the range")
{
	Random random(11u);
	Random reference(11u);
	const float value = random.uniform_float(-2.0f, 6.0f);
	CHECK(value == -2.0f + 8.0f * reference.unit());
	CHECK(value >= -2.0f);
	CHECK(value <= 6.0f);

	CHECK(Random().uniform_float(3.0f, 3.0f) == 3.0f);
}

TEST_CASE("uniform_float refuses a range that is not finite or is reversed")
{
	const float infinity = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float largest = std::numeric_limits<float>::max();

	Random random;
	CHECK_THROWS_AS(random.uniform_float(1.0f, 0.0f), std::invalid_argument);
	CHECK_THROWS_AS(random.uniform_float(0.0f, infinity), std::invalid_argument);
	CHECK_THROWS_AS(random.uniform_float(nan, 1.0f), std::invalid_argument);
	CHECK_THROWS_AS(random.uniform_float(-largest, largest),
		std::invalid_argument);
	CHECK(random == Random());
}

TEST_CASE("chance is never at zero, always at one, and always draws")
{
	Random random(8u);
	Random reference(8u);
	for (int draw = 0; draw < 200; ++draw)
	{
		REQUIRE_FALSE(random.chance(0.0f));
		REQUIRE(random.chance(1.0f));
		reference.next();
		reference.next();
	}
	CHECK(random == reference);

	CHECK_THROWS_AS(random.chance(std::numeric_limits<float>::quiet_NaN()),
		std::invalid_argument);
}

#include <doctest/doctest.h>

#include "samples/collision/pads.h"

#include <array>
#include <cstddef>

using namespace labrador;
using namespace collision;

TEST_CASE("a lone pad in any slot moves the player, resets and quits")
{
	for (const std::size_t occupied : { std::size_t{ 0 }, std::size_t{ 1 }, std::size_t{ 3 } })
	{
		std::array<PadReading, 4> slots;
		slots[occupied] = PadReading{ Vector2F(0.5f, -0.25f), true, true };
		const PadsInput input = combine_pads(slots);
		CHECK(input.stick.x == doctest::Approx(0.5f));
		CHECK(input.stick.y == doctest::Approx(-0.25f));
		CHECK(input.reset_requested);
		CHECK(input.quit_requested);
	}
}

TEST_CASE("several pads add their sticks, and either one's buttons count")
{
	std::array<PadReading, 4> slots;
	slots[0] = PadReading{ Vector2F(-1.0f, 0.0f), false, false };
	slots[2] = PadReading{ Vector2F(0.0f, 1.0f), true, false };
	const PadsInput both = combine_pads(slots);
	CHECK(both.stick.x == doctest::Approx(-1.0f));
	CHECK(both.stick.y == doctest::Approx(1.0f));
	CHECK(both.reset_requested);
	CHECK_FALSE(both.quit_requested);

	// Slot 0 unplugged: it reads as a stick at rest with nothing pressed,
	// and the pad left in slot 2 still steers.
	slots[0] = PadReading{};
	const PadsInput one = combine_pads(slots);
	CHECK(one.stick.x == doctest::Approx(0.0f));
	CHECK(one.stick.y == doctest::Approx(1.0f));
}

TEST_CASE("no pad at all asks for nothing")
{
	const PadsInput input = combine_pads(std::array<PadReading, 4>{});
	CHECK(input.stick.x == 0.0f);
	CHECK(input.stick.y == 0.0f);
	CHECK_FALSE(input.reset_requested);
	CHECK_FALSE(input.quit_requested);
}

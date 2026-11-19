#include <doctest/doctest.h>

#include "samples/components/touch_button.h"

#include <array>
#include <limits>
#include <stdexcept>

using namespace labrador;
using components::TouchButton;

TEST_CASE("a held touch button uses half-open page coordinates and one stable owner")
{
	TouchButton button(RectangleF(10, 20, 100, 80));
	CHECK_FALSE(button.held());
	CHECK(button.contact() == 0);
	CHECK_FALSE(button.begin(0, Vector2F(20, 30)));
	CHECK_FALSE(button.begin(1, Vector2F(110, 30)));
	CHECK_FALSE(button.begin(1, Vector2F(20, 100)));
	CHECK_FALSE(button.begin(1, Vector2F(9, 30)));
	CHECK_FALSE(button.begin(1, Vector2F(20, 19)));
	CHECK(button.begin(7, Vector2F(10, 20)));
	CHECK(button.held());
	CHECK(button.contact() == 7);
	CHECK_FALSE(button.begin(8, Vector2F(40, 40)));
	button.move(8, Vector2F(300, 300));
	button.end(8);
	CHECK(button.held());
	CHECK(button.contact() == 7);
}

TEST_CASE("sliding out releases held state without transferring the touch button")
{
	TouchButton button(RectangleF(-20, -10, 50, 30));
	REQUIRE(button.begin(1, Vector2F::zero));
	button.move(1, Vector2F(30, 0));
	CHECK_FALSE(button.held());
	CHECK(button.contact() == 1);
	CHECK_FALSE(button.begin(2, Vector2F::zero));
	button.move(1, Vector2F(-20, -10));
	CHECK(button.held());
	button.move(1, Vector2F(0, 20));
	CHECK_FALSE(button.held());
	button.move(1, Vector2F(0, 0));
	CHECK(button.held());
	button.end(1);
	CHECK_FALSE(button.held());
	CHECK(button.contact() == 0);
}

TEST_CASE("two button values follow different contacts simultaneously")
{
	TouchButton first(RectangleF(0, 0, 50, 50));
	TouchButton second(RectangleF(100, 0, 50, 50));
	REQUIRE(first.begin(1, Vector2F(25, 25)));
	REQUIRE(second.begin(2, Vector2F(125, 25)));
	first.move(2, Vector2F(1000, 1000));
	second.move(1, Vector2F(-1000, -1000));
	CHECK(first.held());
	CHECK(second.held());
	first.end(1);
	CHECK_FALSE(first.held());
	CHECK(second.held());
	CHECK(second.contact() == 2);
}

TEST_CASE("touch button cancel and end never adopt another already held contact")
{
	TouchButton button(RectangleF(0, 0, 100, 100));
	REQUIRE(button.begin(1, Vector2F(20, 20)));
	CHECK_FALSE(button.begin(2, Vector2F(30, 30)));
	SUBCASE("cancel") { button.cancel(); }
	SUBCASE("release") { button.end(1); }
	button.move(1, Vector2F(20, 20));
	button.move(2, Vector2F(30, 30));
	CHECK_FALSE(button.held());
	CHECK(button.contact() == 0);
	CHECK(button.begin(3, Vector2F(40, 40)));
}

TEST_CASE("touch button geometry changes cancel while identical geometry preserves capture")
{
	TouchButton button(RectangleF(0, 0, 100, 100));
	REQUIRE(button.begin(1, Vector2F(20, 20)));
	button.set_region(button.region());
	CHECK(button.held());
	CHECK(button.contact() == 1);
	button.set_region(RectangleF(10, 10, 100, 100));
	CHECK_FALSE(button.held());
	CHECK(button.contact() == 0);
	button.move(1, Vector2F(20, 20));
	CHECK_FALSE(button.held());
	button.set_region(RectangleF(0, 0, 0, 100));
	CHECK_FALSE(button.begin(2, Vector2F::zero));
	button.set_region(RectangleF(0, 0, 100, 0));
	CHECK_FALSE(button.begin(2, Vector2F::zero));
	TouchButton empty;
	CHECK_FALSE(empty.begin(1, Vector2F::zero));
}

TEST_CASE("bad button geometry and owned positions leave all state unchanged")
{
	const float infinity = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float largest = std::numeric_limits<float>::max();
	TouchButton button(RectangleF(10, 20, 100, 80));
	REQUIRE(button.begin(1, Vector2F(30, 40)));
	for (const RectangleF& invalid : std::array<RectangleF, 8>{
		RectangleF(nan, 20, 100, 80), RectangleF(10, infinity, 100, 80),
		RectangleF(10, 20, infinity, 80), RectangleF(10, 20, 100, nan),
		RectangleF(10, 20, -1, 80), RectangleF(10, 20, 100, -1),
		RectangleF(largest, 20, largest, 80), RectangleF(10, largest, 100, largest) })
	{
		CHECK_THROWS_AS(button.set_region(invalid), std::invalid_argument);
		CHECK(button.region() == RectangleF(10, 20, 100, 80));
		CHECK(button.held());
		CHECK(button.contact() == 1);
	}
	CHECK_THROWS_AS(button.move(1, Vector2F(nan, 40)), std::invalid_argument);
	CHECK_THROWS_AS(button.move(1, Vector2F(30, infinity)), std::invalid_argument);
	CHECK(button.held());
	CHECK(button.contact() == 1);
	CHECK_NOTHROW(button.move(2, Vector2F(nan, nan)));
	CHECK_FALSE(button.begin(2, Vector2F(nan, nan)));
	button.cancel();
	CHECK_THROWS_AS(button.begin(2, Vector2F(infinity, 30)), std::invalid_argument);
	CHECK(button.contact() == 0);
}

TEST_CASE("touch button queries are const and copies own independent held state")
{
	TouchButton original(RectangleF(0, 0, 100, 100));
	REQUIRE(original.begin(1, Vector2F(20, 20)));
	TouchButton copy = original;
	copy.move(1, Vector2F(200, 200));
	const TouchButton& query = original;
	CHECK(query.held());
	CHECK(query.contact() == 1);
	CHECK(query.region() == RectangleF(0, 0, 100, 100));
	CHECK_FALSE(copy.held());
	copy.cancel();
	CHECK(original.held());
}

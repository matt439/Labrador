#include <doctest/doctest.h>

#include "samples/components/touch_stick.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace labrador;
using components::TouchStick;

TEST_CASE("a virtual stick starts only inside its region and keeps its fixed centre")
{
	TouchStick stick(RectangleF(10, 20, 100, 80));
	CHECK(stick.centre() == Vector2F(60, 60));
	CHECK(stick.radius() == 40);
	CHECK(stick.contact() == 0);
	CHECK(stick.value() == Vector2F::zero);
	CHECK_FALSE(stick.begin(0, Vector2F(60, 60)));
	CHECK_FALSE(stick.begin(1, Vector2F(110, 60)));
	CHECK_FALSE(stick.begin(1, Vector2F(60, 100)));
	CHECK_FALSE(stick.begin(1, Vector2F(9, 60)));
	CHECK_FALSE(stick.begin(1, Vector2F(60, 19)));
	CHECK(stick.begin(1, Vector2F(80, 60)));
	CHECK(stick.value() == Vector2F(0.5f, 0));
	CHECK(stick.centre() == Vector2F(60, 60));
	CHECK_FALSE(stick.begin(2, Vector2F(60, 60)));
	stick.move(2, Vector2F(1000, 1000));
	stick.end(2);
	CHECK(stick.contact() == 1);
	CHECK(stick.value() == Vector2F(0.5f, 0));
}

TEST_CASE("the owned stick follows outside its region and clamps radially")
{
	TouchStick stick(RectangleF(10, 20, 100, 80));
	REQUIRE(stick.begin(1, stick.centre()));
	for (const Vector2F& point : std::array<Vector2F, 4>{
		Vector2F(1000, 60), Vector2F(-1000, 60), Vector2F(60, 1000), Vector2F(60, -1000) })
	{
		stick.move(1, point);
		CHECK(stick.contact() == 1);
		CHECK(std::hypot(static_cast<double>(stick.value().x), static_cast<double>(stick.value().y)) <= 1.0);
		CHECK(stick.value().length() == doctest::Approx(1.0f));
	}
	stick.move(1, Vector2F(80, 40));
	CHECK(stick.value() == Vector2F(0.5f, -0.5f));
	stick.move(1, Vector2F(460, 460));
	CHECK(stick.value().x == doctest::Approx(0.70710678f));
	CHECK(stick.value().y == doctest::Approx(0.70710678f));
	stick.move(1, stick.centre());
	CHECK(stick.value() == Vector2F::zero);
	CHECK(stick.contact() == 1);
}

TEST_CASE("stick output stays bounded for large and fractional finite coordinates")
{
	TouchStick stick(RectangleF(-20, -10, 60, 60));
	REQUIRE(stick.begin(1, stick.centre()));
	for (int x = -20; x <= 20; ++x)
	{
		for (int y = -20; y <= 20; ++y)
		{
			stick.move(1, Vector2F(static_cast<float>(x) * 3.17f, static_cast<float>(y) * 2.83f));
			const Vector2F value = stick.value();
			CHECK(std::hypot(static_cast<double>(value.x), static_cast<double>(value.y)) <= 1.0);
		}
	}
	const float largest = std::numeric_limits<float>::max();
	stick.move(1, Vector2F(largest, -largest));
	CHECK(std::isfinite(stick.value().x));
	CHECK(std::isfinite(stick.value().y));
	CHECK(std::hypot(static_cast<double>(stick.value().x), static_cast<double>(stick.value().y)) <= 1.0);
}

TEST_CASE("stick release and cancellation require a later explicit fresh begin")
{
	TouchStick stick(RectangleF(0, 0, 100, 100));
	REQUIRE(stick.begin(1, Vector2F(90, 50)));
	CHECK_FALSE(stick.begin(2, Vector2F(20, 50)));
	SUBCASE("release") { stick.end(1); }
	SUBCASE("cancel") { stick.cancel(); }
	stick.move(1, Vector2F(90, 50));
	stick.move(2, Vector2F(20, 50));
	CHECK(stick.value() == Vector2F::zero);
	CHECK(stick.contact() == 0);
	CHECK(stick.begin(3, Vector2F(50, 90)));
	CHECK(stick.value() == Vector2F(0, 0.8f));
}

TEST_CASE("stick geometry changes cancel instead of reinterpreting a held point")
{
	TouchStick stick(RectangleF(0, 0, 100, 100));
	REQUIRE(stick.begin(1, Vector2F(90, 50)));
	stick.set_region(stick.region());
	CHECK(stick.contact() == 1);
	CHECK(stick.value() == Vector2F(0.8f, 0));
	stick.set_region(RectangleF(100, 200, 200, 100));
	CHECK(stick.value() == Vector2F::zero);
	CHECK(stick.contact() == 0);
	CHECK(stick.centre() == Vector2F(200, 250));
	CHECK(stick.radius() == 50);
	stick.move(1, Vector2F(240, 250));
	CHECK(stick.value() == Vector2F::zero);
	stick.set_region(RectangleF(0, 0, 0, 100));
	CHECK_FALSE(stick.begin(2, Vector2F::zero));
	TouchStick empty;
	CHECK_FALSE(empty.begin(2, Vector2F::zero));
}

TEST_CASE("bad stick geometry and owned positions preserve the old output and ownership")
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();
	const float largest = std::numeric_limits<float>::max();
	const float tiny = std::numeric_limits<float>::denorm_min();
	TouchStick stick(RectangleF(10, 20, 100, 80));
	REQUIRE(stick.begin(1, Vector2F(80, 60)));
	for (const RectangleF& invalid : std::array<RectangleF, 10>{
		RectangleF(nan, 20, 100, 80), RectangleF(10, infinity, 100, 80),
		RectangleF(10, 20, infinity, 80), RectangleF(10, 20, 100, nan),
		RectangleF(10, 20, -1, 80), RectangleF(10, 20, 100, -1),
		RectangleF(largest, 20, largest, 80), RectangleF(10, largest, 100, largest),
		RectangleF(0, 0, tiny, 100),
		RectangleF(largest, 0, largest * std::numeric_limits<float>::epsilon() * 0.125f, 20) })
	{
		CHECK_THROWS_AS(stick.set_region(invalid), std::invalid_argument);
		CHECK(stick.region() == RectangleF(10, 20, 100, 80));
		CHECK(stick.centre() == Vector2F(60, 60));
		CHECK(stick.radius() == 40);
		CHECK(stick.contact() == 1);
		CHECK(stick.value() == Vector2F(0.5f, 0));
	}
	CHECK_THROWS_AS(stick.move(1, Vector2F(nan, 60)), std::invalid_argument);
	CHECK_THROWS_AS(stick.move(1, Vector2F(80, infinity)), std::invalid_argument);
	CHECK(stick.value() == Vector2F(0.5f, 0));
	CHECK(stick.contact() == 1);
	CHECK_NOTHROW(stick.move(2, Vector2F(nan, nan)));
	CHECK_FALSE(stick.begin(2, Vector2F(nan, nan)));
	stick.cancel();
	CHECK_THROWS_AS(stick.begin(2, Vector2F(nan, 60)), std::invalid_argument);
	CHECK(stick.contact() == 0);
}

TEST_CASE("stick copies and const queries carry independent value state")
{
	TouchStick original(RectangleF(0, 0, 100, 100));
	REQUIRE(original.begin(1, Vector2F(90, 50)));
	TouchStick copy = original;
	copy.move(1, Vector2F(50, 10));
	const TouchStick& query = original;
	CHECK(query.value() == Vector2F(0.8f, 0));
	CHECK(query.contact() == 1);
	CHECK(query.region() == RectangleF(0, 0, 100, 100));
	CHECK(query.centre() == Vector2F(50, 50));
	CHECK(query.radius() == 50);
	CHECK(copy.value() == Vector2F(0, -0.8f));
	copy.cancel();
	CHECK(original.value() == Vector2F(0.8f, 0));
}

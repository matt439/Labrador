#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/post_effect.h"

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>

using namespace labrador;

// crt_values and crt_source_position: the CRT's packing and its curvature on
// the CPU, with no device (docs/effects.md#13).

namespace
{
	CrtSettings screen_at(float x, float y, float width, float height)
	{
		CrtSettings settings;
		settings.screen = RectangleF(x, y, width, height);
		return settings;
	}
}

TEST_CASE("crt_values packs the screen as output uv and the terms in order")
{
	CrtSettings settings = screen_at(100.0f, 50.0f, 400.0f, 300.0f);
	settings.curvature = 0.1f;
	settings.scanline_count = 60.0f;
	settings.scanline_strength = 0.5f;
	settings.mask_strength = 0.25f;
	settings.vignette = 0.75f;
	settings.aberration = 2.0f;
	settings.brightness = 1.5f;

	const PostValues packed = crt_values(settings, Vector2F(800.0f, 600.0f));
	CHECK(packed.values[0] == doctest::Approx(0.125f));
	CHECK(packed.values[1] == doctest::Approx(50.0f / 600.0f));
	CHECK(packed.values[2] == doctest::Approx(0.5f));
	CHECK(packed.values[3] == doctest::Approx(0.5f));
	CHECK(packed.values[4] == 0.1f);
	CHECK(packed.values[5] == 60.0f);
	CHECK(packed.values[6] == 0.5f);
	CHECK(packed.values[7] == 0.25f);
	CHECK(packed.values[8] == 0.75f);
	CHECK(packed.values[9] == 2.0f);
	CHECK(packed.values[10] == 1.5f);
	for (int i = 11; i < 32; ++i)
	{
		CHECK(packed.values[static_cast<size_t>(i)] == 0.0f);
	}
}

TEST_CASE("crt_values refuses what the CRT cannot draw")
{
	const Vector2F buffer(640.0f, 480.0f);
	const CrtSettings good = screen_at(0.0f, 0.0f, 640.0f, 480.0f);
	CHECK_NOTHROW(std::ignore = crt_values(good, buffer));

	CrtSettings negative = good;
	negative.vignette = -0.1f;
	CHECK_THROWS_AS(std::ignore = crt_values(negative, buffer),
		std::invalid_argument);

	CrtSettings not_a_number = good;
	not_a_number.curvature = std::numeric_limits<float>::quiet_NaN();
	CHECK_THROWS_AS(std::ignore = crt_values(not_a_number, buffer),
		std::invalid_argument);

	CrtSettings flat = good;
	flat.screen.height = 0.0f;
	CHECK_THROWS_AS(std::ignore = crt_values(flat, buffer),
		std::invalid_argument);

	CHECK_THROWS_AS(std::ignore = crt_values(good, Vector2F(0.0f, 480.0f)),
		std::invalid_argument);

	// 480 / 121 is under four pixels a line; 480 / 120 is exactly four.
	CrtSettings dense = good;
	dense.scanline_strength = 0.5f;
	dense.scanline_count = 121.0f;
	CHECK_THROWS_AS(std::ignore = crt_values(dense, buffer),
		std::invalid_argument);
	dense.scanline_count = 120.0f;
	CHECK_NOTHROW(std::ignore = crt_values(dense, buffer));
	dense.scanline_count = 0.0f;
	CHECK_THROWS_AS(std::ignore = crt_values(dense, buffer),
		std::invalid_argument);

	// With the scanlines off, their count is not checked: a lerp from an
	// opening look to one without scanlines passes through any count.
	dense.scanline_strength = 0.0f;
	dense.scanline_count = 1000.0f;
	CHECK_NOTHROW(std::ignore = crt_values(dense, buffer));
}

TEST_CASE("crt_source_position leaves points outside the screen where they are")
{
	CrtSettings settings = screen_at(100.0f, 100.0f, 200.0f, 100.0f);
	settings.curvature = 0.3f;
	const std::optional<Vector2F> outside =
		crt_source_position(settings, Vector2F(20.0f, 30.0f));
	REQUIRE(outside.has_value());
	CHECK(*outside == Vector2F(20.0f, 30.0f));
}

TEST_CASE("crt_source_position: flat is identity, curved keeps the centre and loses the corners")
{
	CrtSettings settings = screen_at(0.0f, 0.0f, 200.0f, 100.0f);
	const std::optional<Vector2F> flat =
		crt_source_position(settings, Vector2F(37.5f, 81.5f));
	REQUIRE(flat.has_value());
	CHECK(flat->x == doctest::Approx(37.5f));
	CHECK(flat->y == doctest::Approx(81.5f));

	settings.curvature = 0.2f;
	const std::optional<Vector2F> centre =
		crt_source_position(settings, Vector2F(100.0f, 50.0f));
	REQUIRE(centre.has_value());
	CHECK(centre->x == doctest::Approx(100.0f));
	CHECK(centre->y == doctest::Approx(50.0f));

	// A corner is past the tube's edge once there is any curvature.
	CHECK_FALSE(crt_source_position(settings, Vector2F(0.5f, 0.5f)).has_value());
	CHECK_FALSE(
		crt_source_position(settings, Vector2F(199.5f, 99.5f)).has_value());
}

TEST_CASE("crt_source_position: curvature pulls outward, mirror symmetric, monotone along a row")
{
	CrtSettings settings = screen_at(0.0f, 0.0f, 64.0f, 64.0f);
	settings.curvature = 0.1f;

	float previous = -1.0f;
	for (int x = 0; x < 64; ++x)
	{
		const float px = static_cast<float>(x) + 0.5f;
		const std::optional<Vector2F> left =
			crt_source_position(settings, Vector2F(px, 32.0f));
		const std::optional<Vector2F> right =
			crt_source_position(settings, Vector2F(64.0f - px, 32.0f));
		REQUIRE(left.has_value() == right.has_value());
		if (!left.has_value())
		{
			continue;
		}
		CAPTURE(x);
		// Mirror symmetric about the centre column.
		CHECK(left->x + right->x == doctest::Approx(64.0f));
		// Outward: a point left of centre samples further left.
		if (px < 32.0f)
		{
			CHECK(left->x <= px);
		}
		CHECK(left->x > previous);
		previous = left->x;
	}
}

TEST_CASE("crt_values refuses a fraction above 1")
{
	CrtSettings settings = screen_at(0.0f, 0.0f, 640.0f, 480.0f);
	settings.mask_strength = 1.5f;
	CHECK_THROWS_AS(std::ignore = crt_values(settings,
		Vector2F(640.0f, 480.0f)), std::invalid_argument);
	settings.mask_strength = 1.0f;
	CHECK_NOTHROW(std::ignore = crt_values(settings,
		Vector2F(640.0f, 480.0f)));
}

TEST_CASE("crt_values_are_neutral: never set, every term off, or not")
{
	CHECK(crt_values_are_neutral(PostValues{}));

	CrtSettings settings = screen_at(0.0f, 0.0f, 640.0f, 480.0f);
	const Vector2F buffer(640.0f, 480.0f);
	CHECK(crt_values_are_neutral(crt_values(settings, buffer)));

	settings.aberration = 0.5f;
	CHECK_FALSE(crt_values_are_neutral(crt_values(settings, buffer)));
	settings.aberration = 0.0f;
	settings.brightness = 1.1f;
	CHECK_FALSE(crt_values_are_neutral(crt_values(settings, buffer)));
}

TEST_CASE("crt_source_position: a screen with no area maps every point to itself")
{
	const CrtSettings settings;
	const std::optional<Vector2F> origin =
		crt_source_position(settings, Vector2F(0.0f, 0.0f));
	REQUIRE(origin.has_value());
	CHECK(*origin == Vector2F(0.0f, 0.0f));
}

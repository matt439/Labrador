#include <doctest/doctest.h>

#include "engine/collision/resolve.h"

using artattack::separation;
using artattack::separation_along;
using mattmath::Vector2F;

TEST_CASE("separation moves against the normal, by the penetration")
{
	const Vector2F move = separation(Vector2F::DIRECTION_DOWN, 12.0f);

	CHECK(move.x == doctest::Approx(0.0f));
	CHECK(move.y == doctest::Approx(-12.0f));
}

TEST_CASE("separation along an axis closes the overlap, not the distance")
{
	// A 3-4-5 slope, four units deep. Straight up the y axis, that is five
	// units of travel, not four - and getting this wrong is the difference
	// between standing on a ramp and sinking through it a little every frame.
	const Vector2F normal = Vector2F(3.0f, 4.0f).normalized();

	const Vector2F move =
		separation_along(normal, 4.0f, Vector2F::DIRECTION_DOWN);

	CHECK(move.x == doctest::Approx(0.0f));
	CHECK(move.y == doctest::Approx(-5.0f));
}

TEST_CASE("separation along the normal itself is just separation")
{
	const Vector2F normal = Vector2F(1.0f, 2.0f).normalized();

	const Vector2F along = separation_along(normal, 7.0f, normal);
	const Vector2F direct = separation(normal, 7.0f);

	CHECK(along.x == doctest::Approx(direct.x));
	CHECK(along.y == doctest::Approx(direct.y));
}

TEST_CASE("no travel along an axis perpendicular to the normal separates anything")
{
	// Reported as zero rather than as a division by a vanishing dot product,
	// which is an infinity the caller would apply to a position.
	const Vector2F move =
		separation_along(Vector2F::DIRECTION_RIGHT, 10.0f, Vector2F::DIRECTION_DOWN);

	CHECK(move == Vector2F::ZERO);
}

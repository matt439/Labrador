#include <doctest/doctest.h>

#include "engine/collision/resolve.h"

using artattack::separation;
using artattack::separation_along;
using artattack::slide;
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

TEST_CASE("a floor takes the fall and leaves the run alone")
{
	// The whole point of the decomposition: one component dies, the other is
	// not touched. Zeroing the y axis happens to be right here, which is
	// exactly why it survived so long - flat ground is the case where the
	// wrong rule and the right one agree.
	const Vector2F run_and_fall = Vector2F(200.0f, 100.0f);

	const Vector2F after = slide(run_and_fall, Vector2F::DIRECTION_DOWN);

	CHECK(after.x == doctest::Approx(200.0f));
	CHECK(after.y == doctest::Approx(0.0f));
}

TEST_CASE("a slope turns a fall into a slide, not a dead stop")
{
	// Landing on a 3-4-5 ramp at 100 straight down. Deleting the vertical
	// component leaves nothing at all; removing only the component into the
	// slope leaves 60 along it, which is what a player expects to feel.
	const Vector2F normal = Vector2F(3.0f, 4.0f).normalized();

	const Vector2F after = slide(Vector2F(0.0f, 100.0f), normal);

	CHECK(after.x == doctest::Approx(-48.0f));
	CHECK(after.y == doctest::Approx(36.0f));
	CHECK(after.length() == doctest::Approx(60.0f));
}

TEST_CASE("a run into a slope becomes a climb along it")
{
	// Running at 100 along the flat into the same ramp. The result still has
	// speed 80 and it now points up the hill - the climb rate that
	// set_velocity_y(0.0f) deletes, since there is no vertical component yet
	// for it to find.
	const Vector2F normal = Vector2F(3.0f, 4.0f).normalized();

	const Vector2F after = slide(Vector2F(100.0f, 0.0f), normal);

	CHECK(after.x == doctest::Approx(64.0f));
	CHECK(after.y == doctest::Approx(-48.0f));
	CHECK(after.length() == doctest::Approx(80.0f));
}

TEST_CASE("a velocity already leaving the surface is untouched")
{
	// The frame a jump starts, the player is still in contact with the floor.
	// A resolver that clamps regardless would eat the jump.
	const Vector2F jump = Vector2F(0.0f, -300.0f);

	const Vector2F after = slide(jump, Vector2F::DIRECTION_DOWN);

	CHECK(after == jump);
}

TEST_CASE("a velocity straight into the surface is stopped completely")
{
	const Vector2F after = slide(Vector2F(0.0f, 50.0f), Vector2F::DIRECTION_DOWN);

	CHECK(after.x == doctest::Approx(0.0f));
	CHECK(after.y == doctest::Approx(0.0f));
}

TEST_CASE("a velocity already along the surface keeps all of it")
{
	const Vector2F along = Vector2F(150.0f, 0.0f);

	const Vector2F after = slide(along, Vector2F::DIRECTION_DOWN);

	CHECK(after == along);
}

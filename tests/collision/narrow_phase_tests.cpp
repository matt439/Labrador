#include <doctest/doctest.h>

#include "engine/collision/narrow_phase.h"
#include "engine/collision/resolve.h"
#include "engine/math/circle.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

#include <limits>
#include <optional>
#include <stdexcept>

using artattack::Manifold;
using artattack::narrow_phase;
using artattack::separation;
using mattmath::Circle;
using mattmath::Point2F;
using mattmath::RectangleF;
using mattmath::RectangleRotated;
using mattmath::Triangle;
using mattmath::Vector2F;

namespace
{
	// The property every caller depends on and no unit test of the old
	// resolver could have written, because the old resolver's answer was a
	// direction rather than a distance: applying the manifold ends the
	// overlap. Once, exactly, and in one move.
	bool separating(const RectangleF& moving, const RectangleF& fixed)
	{
		const std::optional<Manifold> manifold = narrow_phase(moving, fixed);
		if (!manifold.has_value())
		{
			return true;
		}

		RectangleF moved = moving;
		moved.offset(separation(manifold->normal, manifold->penetration));

		return !narrow_phase(moved, fixed).has_value();
	}
}

TEST_CASE("shapes that are nowhere near each other have no manifold")
{
	const RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);
	const RectangleF b(20.0f, 20.0f, 10.0f, 10.0f);

	CHECK_FALSE(narrow_phase(a, b).has_value());
}

TEST_CASE("shapes that only touch do not overlap")
{
	// A zero-depth contact has no meaningful normal, so it is not a contact.
	const RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);
	const RectangleF b(10.0f, 0.0f, 10.0f, 10.0f);

	CHECK_FALSE(narrow_phase(a, b).has_value());
}

TEST_CASE("the manifold is the axis of least penetration")
{
	// Two across, six down. The way out is across.
	const RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);
	const RectangleF b(8.0f, 4.0f, 10.0f, 10.0f);

	const std::optional<Manifold> manifold = narrow_phase(a, b);

	REQUIRE(manifold.has_value());
	CHECK(manifold->normal == Vector2F::DIRECTION_RIGHT);
	CHECK(manifold->penetration == doctest::Approx(2.0f));
	CHECK(separating(a, b));
}

TEST_CASE("the normal points from the first shape towards the second")
{
	const RectangleF a(0.0f, 0.0f, 10.0f, 10.0f);
	const RectangleF b(0.0f, 8.0f, 10.0f, 10.0f);

	const std::optional<Manifold> forwards = narrow_phase(a, b);
	const std::optional<Manifold> backwards = narrow_phase(b, a);

	REQUIRE(forwards.has_value());
	REQUIRE(backwards.has_value());
	CHECK(forwards->normal == Vector2F::DIRECTION_DOWN);
	CHECK(backwards->normal == Vector2F::DIRECTION_UP);
	CHECK(forwards->penetration == doctest::Approx(backwards->penetration));
}

TEST_CASE("a player impaled on a thin platform is pushed off it, not along it")
{
	// A 60x100 player straddling a 20-tall platform that runs the width of
	// the level, so the platform crosses the player's left and right edges
	// and neither their top nor their bottom.
	//
	// This is the case the eight-way classifier had no answer for. It asked
	// which of the player's bounding-box edges the platform crossed, got
	// "left and right", and fell through to comparing the two centres
	// horizontally - which shoves the player 30 pixels sideways off a
	// platform they are standing on, every frame, while the platform is still
	// under them. The minimum translation is vertical and it is 50.
	const RectangleF player(100.0f, 100.0f, 60.0f, 100.0f);
	const RectangleF platform(0.0f, 150.0f, 800.0f, 20.0f);

	const std::optional<Manifold> manifold = narrow_phase(player, platform);

	REQUIRE(manifold.has_value());
	CHECK(manifold->normal == Vector2F::DIRECTION_DOWN);
	CHECK(manifold->penetration == doctest::Approx(50.0f));
	CHECK(separating(player, platform));
}

TEST_CASE("a shape wholly inside another is pushed out the nearest way")
{
	// The overlap is the whole of the inner shape on both axes, so the width
	// of the intersection is not a distance that separates anything. The two
	// real distances are 30 to the right and 90 to the left.
	const RectangleF outer(0.0f, 0.0f, 100.0f, 100.0f);
	const RectangleF inner(70.0f, 40.0f, 20.0f, 20.0f);

	const std::optional<Manifold> manifold = narrow_phase(inner, outer);

	REQUIRE(manifold.has_value());
	CHECK(manifold->normal == Vector2F::DIRECTION_LEFT);
	CHECK(manifold->penetration == doctest::Approx(30.0f));
	CHECK(separating(inner, outer));
}

TEST_CASE("a ramp resolves along its slope, not along an axis")
{
	// A right triangle with its hypotenuse running from (0, 100) up to
	// (100, 0) - a ramp rising to the right. Nothing about this pair is
	// expressible as one of eight cardinal directions, which is why the ramp
	// responses it replaces were 130 lines of special cases and why left
	// ramps had no response at all until recently.
	const Triangle ramp(Vector2F(0.0f, 100.0f), Vector2F(100.0f, 100.0f),
		Vector2F(100.0f, 0.0f));
	const RectangleF box(60.0f, 40.0f, 20.0f, 20.0f);

	const std::optional<Manifold> manifold = narrow_phase(box, ramp);

	REQUIRE(manifold.has_value());

	// The hypotenuse's own normal, pointing down-right into the ramp.
	const float diagonal = 0.70710678f;
	CHECK(manifold->normal.x == doctest::Approx(diagonal));
	CHECK(manifold->normal.y == doctest::Approx(diagonal));
	CHECK(manifold->penetration == doctest::Approx(28.284271f));
}

TEST_CASE("a circle is rejected rather than silently missed")
{
	// Reporting "no overlap" for a shape the narrow phase cannot measure is
	// the worse failure: it is a collision that never happens and nothing
	// says so. Nothing in the tree is a collidable circle.
	const Circle circle(Vector2F(5.0f, 5.0f), 5.0f);
	const RectangleF rectangle(0.0f, 0.0f, 10.0f, 10.0f);

	CHECK_THROWS_AS(narrow_phase(circle, rectangle), std::invalid_argument);
	CHECK_THROWS_AS(narrow_phase(rectangle, circle), std::invalid_argument);
}

TEST_CASE("one move separates the pair, wherever the overlap is")
{
	// Every relative position of a tall thin box against a wide flat one,
	// which between them cover contained, containing, corner and band.
	const RectangleF fixed(0.0f, 0.0f, 80.0f, 20.0f);

	for (int x = -100; x <= 100; x += 5)
	{
		for (int y = -60; y <= 60; y += 5)
		{
			const RectangleF moving(static_cast<float>(x),
				static_cast<float>(y), 20.0f, 80.0f);

			CAPTURE(x);
			CAPTURE(y);
			REQUIRE(separating(moving, fixed));
		}
	}
}

TEST_CASE("one move still separates the pair out where the levels actually are")
{
	// The same sweep as above, translated to the coordinates the game runs at.
	//
	// Every other collision test in this file sits within 100 units of the
	// origin, where floats are dense and everything works. Levels run to
	// around 6200, where consecutive floats are 4.88e-4 apart - which is
	// larger than mattmath::EPSILON (1e-4), so the engine's general tolerance
	// is exact equality wearing a costume out here.
	//
	// This is the measurement that decides whether resolution needs a contact
	// skin - a small extra push so a resting object does not re-collide with
	// what it is standing on every frame. The honest way to answer that is to
	// run the arithmetic at the magnitudes it will see, rather than to add a
	// fudge factor speculatively and never learn whether it was needed (T3).
	// 6200 is where this game's levels end. The larger origins are headroom,
	// and they are here so that the answer is a bound rather than a lucky
	// coincidence at one magnitude.
	const float origins[] = { 6000.0f, 60000.0f, 600000.0f };

	for (const float origin : origins)
	{
		const RectangleF fixed(origin, origin, 80.0f, 20.0f);

		for (int x = -100; x <= 100; x += 5)
		{
			for (int y = -60; y <= 60; y += 5)
			{
				const RectangleF moving(origin + static_cast<float>(x),
					origin + static_cast<float>(y), 20.0f, 80.0f);

				CAPTURE(origin);
				CAPTURE(x);
				CAPTURE(y);
				REQUIRE(separating(moving, fixed));
			}
		}
	}
}

TEST_CASE("a collinear triangle contains nothing, so it contacts nothing")
{
	// Triangle's constructor validates nothing, so a triangle with no interior
	// is constructible today. Its three edge normals are all parallel and all
	// perfectly well formed, so skipping degenerate edges does not catch it:
	// projected onto any of them the triangle is a single point, and both ways
	// off the axis are positive whenever the box straddles that line. It
	// reported a confident overlap against a shape with no area.
	const Triangle flat(Point2F(0.0f, 0.0f), Point2F(5.0f, 0.0f),
		Point2F(10.0f, 0.0f));
	const RectangleF box(-5.0f, -5.0f, 20.0f, 20.0f);

	CHECK_FALSE(narrow_phase(flat, box).has_value());
	CHECK_FALSE(narrow_phase(box, flat).has_value());

	// A very thin triangle is a real shape, not a degenerate one, and is
	// still measured.
	const Triangle thin(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
		Point2F(0.0f, 0.01f));
	CHECK(narrow_phase(thin, box).has_value());
}

TEST_CASE("a NaN coordinate reports no contact, rather than a NaN one")
{
	// The axis test decides overlap by asking whether both ways off the axis
	// are positive. Written as "neither is non-positive" it reached the
	// overlapping branch by falling through, because every comparison against
	// NaN is false - so a poisoned coordinate produced a manifold carrying a
	// NaN penetration, which a resolver would then apply to a position and
	// spread to everything that object later touched.
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const RectangleF good(0.0f, 0.0f, 10.0f, 10.0f);

	CHECK_FALSE(narrow_phase(RectangleF(nan, 0.0f, 10.0f, 10.0f), good)
		.has_value());
	CHECK_FALSE(narrow_phase(good, RectangleF(nan, 0.0f, 10.0f, 10.0f))
		.has_value());

	// A NaN extent, not only a NaN position.
	CHECK_FALSE(narrow_phase(RectangleF(0.0f, 0.0f, nan, 10.0f), good)
		.has_value());
}

TEST_CASE("a rotated rectangle is measured without being turned into a Quad")
{
	// Two distinct axes, read off the shape's own corners. This pair used to
	// build a mattmath::Quad per shape per pair, which allocates and validates
	// twice over - inside the function whose comment says it carries points in
	// an array precisely to avoid a heap allocation per shape per pair.
	const float diagonal = 0.70710678f;
	const RectangleRotated tilted(Point2F(0.0f, 0.0f),
		Point2F(diagonal, diagonal), Point2F(-diagonal, diagonal),
		Point2F(10.0f, 10.0f));

	const std::optional<Manifold> hit =
		narrow_phase(tilted, RectangleF(-2.0f, -2.0f, 4.0f, 4.0f));
	REQUIRE(hit.has_value());
	CHECK(hit->penetration > 0.0f);

	CHECK_FALSE(narrow_phase(tilted,
		RectangleF(100.0f, 100.0f, 4.0f, 4.0f)).has_value());
}

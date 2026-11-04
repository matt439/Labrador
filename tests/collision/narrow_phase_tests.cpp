#include <doctest/doctest.h>

#include "engine/collision/narrow_phase.h"
#include "engine/collision/resolve.h"
#include "engine/core/random.h"
#include "engine/math/circle.h"
#include "engine/math/intersects.h"
#include "engine/math/quad.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

#include <cmath>
#include <limits>
#include <optional>

using labrador::Manifold;
using labrador::narrow_phase;
using labrador::separation;
using labrador::Circle;
using labrador::Point2F;
using labrador::RectangleF;
using labrador::RectangleRotated;
using labrador::Triangle;
using labrador::Vector2F;

namespace
{
	// The property every caller depends on, which needs the answer to be a
	// distance rather than a direction: applying the manifold ends the
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
	CHECK(manifold->normal == Vector2F::direction_right);
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
	CHECK(forwards->normal == Vector2F::direction_down);
	CHECK(backwards->normal == Vector2F::direction_up);
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
	CHECK(manifold->normal == Vector2F::direction_down);
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
	CHECK(manifold->normal == Vector2F::direction_left);
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
	// larger than labrador::default_epsilon (1e-4), so the engine's general tolerance
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
	// Two distinct axes, read off the shape's own corners rather than out of a
	// labrador::Quad built per shape per pair, which allocates and validates
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

TEST_CASE("thin rotated rectangles retain both separating axes")
{
	const RectangleRotated first(Point2F(0.0f, 0.0f),
		Vector2F(1.0f, 0.0f), Vector2F(0.0f, 1.0f), Vector2F(0.0001f, 1.0f));
	const RectangleRotated second(Point2F(0.0f, 100.0f),
		Vector2F(1.0f, 0.0f), Vector2F(0.0f, 1.0f), Vector2F(0.0001f, 1.0f));
	CHECK_FALSE(narrow_phase(first, second).has_value());
	CHECK_FALSE(narrow_phase(second, first).has_value());
	CHECK(narrow_phase(first, first).has_value());
}

TEST_CASE("tiny nonzero polygon edges still contribute separating axes")
{
	for (const float size : { 0.0001f, 1.0e-20f })
	{
		const Triangle first(Point2F(0.0f, 0.0f), Point2F(size, 0.0f),
			Point2F(0.0f, size));
		const Triangle far(Point2F(size * 2.0f, 0.0f), Point2F(size * 3.0f, 0.0f),
			Point2F(size * 2.0f, size));
		CHECK(narrow_phase(first, first).has_value());
		CHECK_FALSE(narrow_phase(first, far).has_value());
	}
	const labrador::Quad first(Point2F(0.0f, 0.0f), Point2F(0.0001f, 0.0f),
		Point2F(0.0001f, 1.0f), Point2F(0.0f, 1.0f));
	const labrador::Quad second(Point2F(0.0f, 2.0f), Point2F(0.0001f, 2.0f),
		Point2F(0.0001f, 3.0f), Point2F(0.0f, 3.0f));
	CHECK_FALSE(narrow_phase(first, second).has_value());
}

TEST_CASE("nonfinite geometry is rejected even inside an otherwise containing polygon")
{
	const RectangleF outer(0.0f, 0.0f, 20.0f, 20.0f);
	for (const float poison : { std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
	{
		for (const RectangleF& invalid : { RectangleF(poison, 5.0f, 10.0f, 10.0f),
			RectangleF(5.0f, poison, 10.0f, 10.0f),
			RectangleF(5.0f, 5.0f, poison, 10.0f),
			RectangleF(5.0f, 5.0f, 10.0f, poison) })
		{
			CHECK_FALSE(narrow_phase(invalid, outer).has_value());
			CHECK_FALSE(narrow_phase(outer, invalid).has_value());
		}
		for (int vertex = 0; vertex < 3; vertex++)
		{
			for (int coordinate = 0; coordinate < 2; coordinate++)
			{
				Point2F points[] = { Point2F(5.0f, 5.0f), Point2F(15.0f, 5.0f),
					Point2F(5.0f, 15.0f) };
				if (coordinate == 0) { points[vertex].x = poison; }
				else { points[vertex].y = poison; }
				const Triangle invalid(points[0], points[1], points[2]);
				CHECK_FALSE(narrow_phase(invalid, outer).has_value());
				CHECK_FALSE(narrow_phase(outer, invalid).has_value());
			}
		}
	}
}

TEST_CASE("two overlapping circles meet along the line between their centres")
{
	const Circle a(Vector2F(0.0f, 0.0f), 5.0f);
	const Circle b(Vector2F(6.0f, 8.0f), 7.0f);

	const std::optional<Manifold> manifold = narrow_phase(a, b);
	REQUIRE(manifold.has_value());
	CHECK(manifold->normal.x == doctest::Approx(0.6f));
	CHECK(manifold->normal.y == doctest::Approx(0.8f));
	CHECK(manifold->penetration == doctest::Approx(2.0f));

	const std::optional<Manifold> reversed = narrow_phase(b, a);
	REQUIRE(reversed.has_value());
	CHECK(reversed->normal.x == doctest::Approx(-0.6f));
	CHECK(reversed->normal.y == doctest::Approx(-0.8f));
	CHECK(reversed->penetration == doctest::Approx(2.0f));
}

TEST_CASE("circles that only touch, or are apart, do not overlap")
{
	const Circle a(Vector2F(0.0f, 0.0f), 5.0f);

	CHECK_FALSE(narrow_phase(a, Circle(Vector2F(10.0f, 0.0f), 5.0f)).has_value());
	CHECK_FALSE(narrow_phase(a, Circle(Vector2F(6.0f, 8.0f), 5.0f)).has_value());
	CHECK_FALSE(narrow_phase(a, Circle(Vector2F(50.0f, 0.0f), 5.0f)).has_value());
}

TEST_CASE("concentric circles separate along positive x by their radii")
{
	// Every direction is a least-penetration axis, so the answer is a fixed
	// one rather than whatever a zero-length normalisation produces.
	const Circle outer(Vector2F(3.0f, 4.0f), 10.0f);
	const Circle inner(Vector2F(3.0f, 4.0f), 2.0f);

	const std::optional<Manifold> manifold = narrow_phase(inner, outer);
	REQUIRE(manifold.has_value());
	CHECK(manifold->normal == Vector2F(1.0f, 0.0f));
	CHECK(manifold->penetration == doctest::Approx(12.0f));
}

TEST_CASE("a circle beside a box's face is pushed straight off that face")
{
	const RectangleF box(0.0f, 0.0f, 10.0f, 10.0f);
	const Circle circle(Vector2F(-3.0f, 5.0f), 5.0f);

	const std::optional<Manifold> manifold = narrow_phase(circle, box);
	REQUIRE(manifold.has_value());
	CHECK(manifold->normal.x == doctest::Approx(1.0f));
	CHECK(manifold->normal.y == doctest::Approx(0.0f));
	CHECK(manifold->penetration == doctest::Approx(2.0f));

	const std::optional<Manifold> reversed = narrow_phase(box, circle);
	REQUIRE(reversed.has_value());
	CHECK(reversed->normal.x == doctest::Approx(-1.0f));
	CHECK(reversed->normal.y == doctest::Approx(0.0f));
	CHECK(reversed->penetration == doctest::Approx(2.0f));
}

TEST_CASE("a circle against a box's corner is pushed away from the corner")
{
	// Neither face normal is the answer here: separating along either one
	// travels further than the circle overlaps. The corner-to-centre axis is
	// the one that measures it, at radius minus distance.
	const RectangleF box(0.0f, 0.0f, 10.0f, 10.0f);
	const Circle circle(Vector2F(13.0f, 14.0f), 6.0f);

	const std::optional<Manifold> manifold = narrow_phase(circle, box);
	REQUIRE(manifold.has_value());
	CHECK(manifold->normal.x == doctest::Approx(-0.6f));
	CHECK(manifold->normal.y == doctest::Approx(-0.8f));
	CHECK(manifold->penetration == doctest::Approx(1.0f));

	// The same distance outside the corner, and no overlap: the boxes overlap
	// but the shapes do not, which is the case a bounding-box test gets wrong.
	const Circle clear(Vector2F(13.0f, 14.0f), 4.5f);
	REQUIRE(clear.bounding_box().intersects(box));
	CHECK_FALSE(narrow_phase(clear, box).has_value());
	CHECK_FALSE(narrow_phase(box, clear).has_value());
}

TEST_CASE("a circle grazing a face does not overlap it")
{
	const RectangleF box(0.0f, 0.0f, 10.0f, 10.0f);

	CHECK_FALSE(narrow_phase(Circle(Vector2F(-5.0f, 5.0f), 5.0f), box).has_value());
	CHECK_FALSE(narrow_phase(box, Circle(Vector2F(5.0f, 15.0f), 5.0f)).has_value());
}

TEST_CASE("a circle contained by a polygon, or containing one, leaves the nearest way")
{
	const RectangleF box(0.0f, 0.0f, 20.0f, 10.0f);

	// Centre inside, nearer the top face: out through the top, by the
	// distance to it plus the whole radius.
	const std::optional<Manifold> inside =
		narrow_phase(Circle(Vector2F(8.0f, 3.0f), 1.0f), box);
	REQUIRE(inside.has_value());
	CHECK(inside->normal.x == doctest::Approx(0.0f));
	CHECK(inside->normal.y == doctest::Approx(1.0f));
	CHECK(inside->penetration == doctest::Approx(4.0f));

	// A circle swallowing the box: the box's narrow dimension wins.
	const std::optional<Manifold> around =
		narrow_phase(Circle(Vector2F(10.0f, 5.0f), 50.0f), box);
	REQUIRE(around.has_value());
	CHECK(around->normal.x == doctest::Approx(0.0f));
	CHECK(std::abs(around->normal.y) == doctest::Approx(1.0f));
	CHECK(around->penetration == doctest::Approx(55.0f));

	// A centre exactly on a vertex has no direction to that vertex, and the
	// faces measure it.
	const std::optional<Manifold> on_corner =
		narrow_phase(Circle(Vector2F(0.0f, 0.0f), 2.0f), box);
	REQUIRE(on_corner.has_value());
	CHECK(on_corner->penetration == doctest::Approx(2.0f));
}

TEST_CASE("a circle is measured against every polygon type")
{
	const Circle circle(Vector2F(0.0f, 0.0f), 2.0f);

	// A triangle whose slanted face is nearest the circle: that face's
	// normal is the axis, which no box test can produce.
	const Triangle triangle(Point2F(2.5f, 0.0f), Point2F(0.0f, 2.5f),
		Point2F(4.0f, 4.0f));
	const std::optional<Manifold> from_triangle = narrow_phase(circle, triangle);
	REQUIRE(from_triangle.has_value());
	CHECK(from_triangle->normal.x == doctest::Approx(0.70710678f));
	CHECK(from_triangle->normal.y == doctest::Approx(0.70710678f));
	CHECK(from_triangle->penetration == doctest::Approx(2.0f - 1.76776695f));
	CHECK_FALSE(narrow_phase(circle, Triangle(Point2F(3.0f, 0.0f),
		Point2F(0.0f, 3.0f), Point2F(4.0f, 4.0f))).has_value());

	// A kite whose nearest feature is a vertex, so the vertex axis decides.
	const labrador::Quad kite(Point2F(1.0f, 0.0f), Point2F(4.0f, -2.0f),
		Point2F(7.0f, 0.0f), Point2F(4.0f, 2.0f));
	const std::optional<Manifold> from_quad = narrow_phase(kite, circle);
	REQUIRE(from_quad.has_value());
	CHECK(from_quad->normal.x == doctest::Approx(-1.0f));
	CHECK(from_quad->normal.y == doctest::Approx(0.0f));
	CHECK(from_quad->penetration == doctest::Approx(1.0f));
	CHECK_FALSE(narrow_phase(circle, labrador::Quad(Point2F(2.5f, 0.0f),
		Point2F(4.0f, -2.0f), Point2F(7.0f, 0.0f), Point2F(4.0f, 2.0f)))
		.has_value());

	const float diagonal = 0.70710678f;
	const RectangleRotated tilted(Point2F(2.0f, 2.0f),
		Point2F(diagonal, diagonal), Point2F(-diagonal, diagonal),
		Point2F(1.0f, 3.0f));
	const std::optional<Manifold> from_rotated = narrow_phase(circle, tilted);
	REQUIRE(from_rotated.has_value());
	CHECK(from_rotated->normal.x == doctest::Approx(diagonal));
	CHECK(from_rotated->normal.y == doctest::Approx(diagonal));
	CHECK(from_rotated->penetration
		== doctest::Approx(3.0f - 2.0f * 1.41421356f).epsilon(0.001));
}

TEST_CASE("a circle with no area, or a nonfinite one, contacts nothing")
{
	// inflate() can take a radius to zero or below. Neither encloses
	// anything, and neither may hand a resolver a distance.
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();
	const RectangleF box(0.0f, 0.0f, 10.0f, 10.0f);
	const Circle good(Vector2F(5.0f, 5.0f), 3.0f);

	const Circle invalid[] = {
		Circle(Vector2F(5.0f, 5.0f), 0.0f),
		Circle(Vector2F(5.0f, 5.0f), -3.0f),
		Circle(Vector2F(nan, 5.0f), 3.0f),
		Circle(Vector2F(5.0f, infinity), 3.0f),
		Circle(Vector2F(5.0f, 5.0f), nan),
		Circle(Vector2F(5.0f, 5.0f), infinity),
	};
	for (const Circle& circle : invalid)
	{
		CHECK_FALSE(narrow_phase(circle, box).has_value());
		CHECK_FALSE(narrow_phase(box, circle).has_value());
		CHECK_FALSE(narrow_phase(circle, good).has_value());
		CHECK_FALSE(narrow_phase(good, circle).has_value());
	}

	CHECK_FALSE(narrow_phase(good, RectangleF(nan, 0.0f, 10.0f, 10.0f))
		.has_value());
	CHECK_FALSE(narrow_phase(good, Triangle(Point2F(0.0f, 0.0f),
		Point2F(5.0f, 5.0f), Point2F(10.0f, 10.0f))).has_value());
}

namespace
{
	// A random circle and one of each polygon type, near each other.
	struct CirclePair
	{
		Circle circle;
		RectangleF rectangle;
		Triangle triangle;
		labrador::Quad quad;
		RectangleRotated rotated;
	};

	CirclePair random_pair(labrador::Random& random, float origin)
	{
		const auto coordinate = [&random, origin]()
		{
			return origin + random.uniform_float(-12.0f, 12.0f);
		};

		CirclePair pair;
		pair.circle = Circle(Vector2F(coordinate(), coordinate()),
			random.uniform_float(0.5f, 8.0f));
		pair.rectangle = RectangleF(origin + random.uniform_float(-8.0f, 0.0f),
			origin + random.uniform_float(-8.0f, 0.0f),
			random.uniform_float(1.0f, 10.0f), random.uniform_float(1.0f, 10.0f));

		// Counter-clockwise around the origin at three increasing angles,
		// so the triangle is never degenerate.
		const float start = random.uniform_float(0.0f, 2.0f);
		const auto at = [&random, origin](float angle)
		{
			const float length = random.uniform_float(2.0f, 8.0f);
			return Point2F(origin + length * std::cos(angle),
				origin + length * std::sin(angle));
		};
		pair.triangle = Triangle(at(start), at(start + 2.0f), at(start + 4.0f));

		// A rhombus with random half-diagonals, convex by construction.
		const float across = random.uniform_float(1.0f, 8.0f);
		const float down = random.uniform_float(1.0f, 8.0f);
		pair.quad = labrador::Quad(Point2F(origin - across, origin),
			Point2F(origin, origin - down), Point2F(origin + across, origin),
			Point2F(origin, origin + down));

		const float angle = random.uniform_float(0.0f, 3.14159265f);
		const Vector2F x_axis(std::cos(angle), std::sin(angle));
		const Vector2F y_axis(-x_axis.y, x_axis.x);
		// Built at zero and moved, because the constructor's corner checks
		// are stricter than the far-from-zero rounding of its own corners.
		pair.rotated = RectangleRotated(Point2F(0.0f, 0.0f), x_axis, y_axis,
			Vector2F(random.uniform_float(0.5f, 6.0f),
				random.uniform_float(0.5f, 6.0f)));
		pair.rotated.offset(Vector2F(origin, origin));
		return pair;
	}

	// The narrow phase's answer must agree with intersects.h's yes/no
	// predicate, which is closed where the narrow phase is open: a contact
	// implies the predicate, and the predicate for a slightly smaller circle
	// implies a contact. Between the two is only the grazing boundary.
	//
	// The manifold must also be the least translation: moving the circle a
	// little more than the penetration along the normal separates the pair,
	// and moving it a little less does not.
	void check_against(const Circle& circle, const labrador::Shape& polygon,
		bool (*predicate)(const Circle&, const labrador::Shape&), float origin)
	{
		const float tolerance = 1.0e-4f * (1.0f + std::abs(origin));
		const std::optional<Manifold> manifold = narrow_phase(circle, polygon);

		Circle smaller = circle;
		smaller.inflate(-tolerance);
		if (predicate(smaller, polygon))
		{
			REQUIRE(manifold.has_value());
		}
		if (!manifold.has_value())
		{
			return;
		}
		REQUIRE(predicate(circle, polygon));

		CHECK(manifold->normal.length() == doctest::Approx(1.0f));
		REQUIRE(manifold->penetration > 0.0f);

		const std::optional<Manifold> reversed = narrow_phase(polygon, circle);
		REQUIRE(reversed.has_value());
		CHECK(reversed->normal == -manifold->normal);
		CHECK(reversed->penetration == manifold->penetration);

		Circle beyond = circle;
		beyond.offset(separation(manifold->normal,
			manifold->penetration + tolerance));
		CHECK_FALSE(narrow_phase(beyond, polygon).has_value());

		if (manifold->penetration > 4.0f * tolerance)
		{
			Circle short_of = circle;
			short_of.offset(separation(manifold->normal,
				manifold->penetration - 2.0f * tolerance));
			CHECK(narrow_phase(short_of, polygon).has_value());
		}
	}
}

TEST_CASE("circle manifolds agree with intersects.h and are the least translation")
{
	using labrador::Shape;
	const auto rectangle = [](const Circle& circle, const Shape& shape)
	{
		return labrador::rectangle_circle_intersect(
			static_cast<const RectangleF&>(shape), circle);
	};
	const auto triangle = [](const Circle& circle, const Shape& shape)
	{
		return labrador::circle_triangle_intersect(circle,
			static_cast<const Triangle&>(shape));
	};
	const auto quad = [](const Circle& circle, const Shape& shape)
	{
		return labrador::circle_quad_intersect(circle,
			static_cast<const labrador::Quad&>(shape));
	};
	const auto rotated = [](const Circle& circle, const Shape& shape)
	{
		return labrador::circle_rectangle_rotated_intersect(circle,
			static_cast<const RectangleRotated&>(shape));
	};
	const auto circles = [](const Circle& circle, const Shape& shape)
	{
		return labrador::circles_intersect(circle,
			static_cast<const Circle&>(shape));
	};

	labrador::Random random(7u);
	int contacts = 0;
	for (const float origin : { 0.0f, 6000.0f })
	{
		for (int i = 0; i < 2000; i++)
		{
			const CirclePair pair = random_pair(random, origin);
			const Circle other(pair.rotated.center(),
				random.uniform_float(0.5f, 8.0f));
			CAPTURE(origin);
			CAPTURE(i);
			check_against(pair.circle, pair.rectangle, rectangle, origin);
			check_against(pair.circle, pair.triangle, triangle, origin);
			check_against(pair.circle, pair.quad, quad, origin);
			check_against(pair.circle, pair.rotated, rotated, origin);
			check_against(pair.circle, other, circles, origin);
			contacts += narrow_phase(pair.circle, pair.rectangle).has_value() ? 1 : 0;
		}
	}

	// Both outcomes are exercised, not only one of them.
	CHECK(contacts > 500);
	CHECK(contacts < 3500);
}

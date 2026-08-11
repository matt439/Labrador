#include <doctest/doctest.h>

#include "engine/math/matt_math.h"

#include <stdexcept>

using namespace mattmath;

TEST_SUITE("Quad")
{
	TEST_CASE("a rejected Quad setter leaves the quad as it was")
	{
		// The sibling of "test_rectangle_rotated_setters_are_transactional".
		// Quad assigned the member first and validated afterwards, so a
		// rejected point stayed in the object - and because is_valid() is
		// a convexity test over all four points, the quad was then stuck:
		// every subsequent setter rejected the shape it had been left in,
		// so it could not be repaired through its own API.
		Quad q(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f));
		const Quad before = q;

		// Pushing point 1 through the far diagonal makes a dart.
		CHECK_THROWS_AS(q.set_point_1(Point2F(5.0f, 5.0f)),
			std::invalid_argument);
		CHECK(q == before);

		// Still usable afterwards: the object was not left corrupt.
		q.set_point_1(Point2F(20.0f, 0.0f));
		CHECK(q.point_1() == Point2F(20.0f, 0.0f));
		CHECK(q.contains(Point2F(5.0f, 5.0f)));
	}
	TEST_CASE("a Quad must be convex, not merely non-self-intersecting")
	{
		// A dart: the square with one corner pushed back through the
		// opposite diagonal. No two of its edges cross, so the old
		// simplicity test accepted it - and then the separating-axis
		// theorem, which only decides convex shapes, produced a confident
		// wrong manifold for it.
		CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(2.0f, 2.0f), Point2F(0.0f, 10.0f)),
			std::invalid_argument);

		// A bowtie, which the old test also caught, since its edges do
		// cross.
		CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(0.0f, 10.0f), Point2F(10.0f, 10.0f)),
			std::invalid_argument);

		// Degenerate: three points on a line, and a repeated vertex. Both
		// put a zero on one side of a diagonal, and a zero is not strictly
		// opposite anything.
		CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(5.0f, 0.0f),
			Point2F(10.0f, 0.0f), Point2F(0.0f, 10.0f)),
			std::invalid_argument);
		CHECK_THROWS_AS(Quad(Point2F(0.0f, 0.0f), Point2F(0.0f, 0.0f),
			Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f)),
			std::invalid_argument);

		// Convex quads are still fine, wound either way.
		CHECK_NOTHROW(Quad(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f)));
		CHECK_NOTHROW(Quad(Point2F(0.0f, 10.0f), Point2F(10.0f, 10.0f),
			Point2F(10.0f, 0.0f), Point2F(0.0f, 0.0f)));
	}
}

#include <doctest/doctest.h>

#include "engine/math/quad.h"
#include "engine/math/rectanglef.h"
#include "engine/math/triangle.h"
#include "engine/math/vector2f.h"

using namespace mattmath;

TEST_SUITE("RectangleF")
{
	TEST_CASE("bounding_box_of is the one extremal fold")
	{
		// It was written three times - Triangle, Quad and
		// RectangleRotated - and the last two were character for
		// character identical.
		const Point2F points[4] = { Point2F(3.0f, -2.0f),
			Point2F(-1.0f, 5.0f), Point2F(7.0f, 1.0f),
			Point2F(2.0f, 4.0f) };

		const RectangleF box = RectangleF::bounding_box_of(points);

		CHECK(box.left() == -1.0f);
		CHECK(box.top() == -2.0f);
		CHECK(box.right() == 7.0f);
		CHECK(box.bottom() == 5.0f);

		// Every shape that has one agrees with it.
		const Triangle tri(Point2F(3.0f, -2.0f), Point2F(-1.0f, 5.0f),
			Point2F(7.0f, 1.0f));
		const Point2F three[3] = { tri.point_0(), tri.point_1(),
			tri.point_2() };
		CHECK(tri.bounding_box() == RectangleF::bounding_box_of(three));

		const Quad q(Point2F(0.0f, 0.0f), Point2F(10.0f, 0.0f),
			Point2F(10.0f, 10.0f), Point2F(0.0f, 10.0f));
		CHECK(q.bounding_box() == RectangleF(0.0f, 0.0f, 10.0f, 10.0f));

		// A single point is a box with no extent, and nothing at all is
		// ZERO rather than a guess.
		const Point2F one[1] = { Point2F(4.0f, 9.0f) };
		CHECK(RectangleF::bounding_box_of(one) ==
			RectangleF(4.0f, 9.0f, 0.0f, 0.0f));
		CHECK(RectangleF::bounding_box_of({}) == RectangleF::ZERO);
	}
}

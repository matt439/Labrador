#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"

using namespace labrador;

// The behaviour rectanglei.h and vector2i.h state, pinned as it is. What
// these types do with non-finite, out-of-range or zero-divisor input is
// not stated, so nothing here exercises it.

TEST_SUITE("Vector2I")
{
	TEST_CASE("converting from Vector2F truncates each component toward zero")
	{
		CHECK(Vector2I(Vector2F(2.9f, -2.9f)) == Vector2I(2, -2));
		CHECK(Vector2I(Vector2F(0.5f, -0.5f)) == Vector2I(0, 0));
		CHECK(Vector2I(Vector2F(-1.0f, 3.0f)) == Vector2I(-1, 3));
	}

	TEST_CASE("division truncates toward zero per component")
	{
		CHECK(Vector2I(7, -7) / 2 == Vector2I(3, -3));
		CHECK(Vector2I(7, -7) / Vector2I(2, 3) == Vector2I(3, -2));

		Vector2I by_scalar(9, -9);
		by_scalar /= 4;
		CHECK(by_scalar == Vector2I(2, -2));

		Vector2I by_vector(9, -9);
		by_vector /= Vector2I(-2, 2);
		CHECK(by_vector == Vector2I(-4, -4));
	}

	TEST_CASE("addition, subtraction and multiplication are per component")
	{
		CHECK(Vector2I(1, 2) + Vector2I(10, -20) == Vector2I(11, -18));
		CHECK(Vector2I(1, 2) - Vector2I(10, -20) == Vector2I(-9, 22));
		CHECK(Vector2I(3, -4) * Vector2I(2, 5) == Vector2I(6, -20));
		CHECK(Vector2I(3, -4) * 3 == Vector2I(9, -12));
		CHECK(3 * Vector2I(3, -4) == Vector2I(9, -12));

		Vector2I v(1, 1);
		v += Vector2I(2, 3);
		CHECK(v == Vector2I(3, 4));
		v -= Vector2I(1, 5);
		CHECK(v == Vector2I(2, -1));
		v *= Vector2I(4, 2);
		CHECK(v == Vector2I(8, -2));
		v *= -1;
		CHECK(v == Vector2I(-8, 2));
	}

	TEST_CASE("offset, scale and set change the components they name")
	{
		Vector2I v(5, 6);
		v.offset(-2, 3);
		CHECK(v == Vector2I(3, 9));
		v.scale(2, -1);
		CHECK(v == Vector2I(6, -9));
		v.set(0, 7);
		CHECK(v == Vector2I(0, 7));
		CHECK(v != Vector2I::zero);
		CHECK(Vector2I() == Vector2I::zero);
	}
}

TEST_SUITE("RectangleI")
{
	TEST_CASE("converting from floats truncates each field toward zero")
	{
		CHECK(RectangleI(RectangleF(0.5f, 0.5f, 2.9f, 2.9f)) ==
			RectangleI(0, 0, 2, 2));
		CHECK(RectangleI(RectangleF(-1.5f, -0.9f, 3.99f, 1.01f)) ==
			RectangleI(-1, 0, 3, 1));
		CHECK(RectangleI(Vector2F(-2.7f, 4.2f), Vector2F(5.5f, 0.9f)) ==
			RectangleI(-2, 4, 5, 0));
		CHECK(RectangleI(Vector2I(-2, 4), Vector2I(5, 1)) ==
			RectangleI(-2, 4, 5, 1));
	}

	TEST_CASE("edges and corners come from x, y, width and height")
	{
		const RectangleI r(10, 20, 30, 40);

		CHECK(r.left() == 10);
		CHECK(r.top() == 20);
		CHECK(r.right() == 40);
		CHECK(r.bottom() == 60);
		CHECK(r.position() == Vector2I(10, 20));
		CHECK(r.size() == Vector2I(30, 40));
		CHECK(r.top_left() == Vector2I(10, 20));
		CHECK(r.bottom_right() == Vector2I(40, 60));
	}

	TEST_CASE("contains a point inclusively at each edge")
	{
		const RectangleI r(10, 20, 30, 40);

		CHECK(r.contains(Vector2I(25, 40)));

		CHECK(r.contains(Vector2I(10, 40)));
		CHECK(r.contains(Vector2I(40, 40)));
		CHECK(r.contains(Vector2I(25, 20)));
		CHECK(r.contains(Vector2I(25, 60)));
		CHECK(r.contains(r.top_left()));
		CHECK(r.contains(r.bottom_right()));

		CHECK_FALSE(r.contains(Vector2I(9, 40)));
		CHECK_FALSE(r.contains(Vector2I(41, 40)));
		CHECK_FALSE(r.contains(Vector2I(25, 19)));
		CHECK_FALSE(r.contains(Vector2I(25, 61)));

		// With no extent the rectangle still holds its own corner.
		const RectangleI empty(3, 4, 0, 0);
		CHECK(empty.contains(Vector2I(3, 4)));
		CHECK_FALSE(empty.contains(Vector2I(4, 4)));
	}

	TEST_CASE("contains a rectangle inclusively at each edge")
	{
		const RectangleI r(10, 20, 30, 40);

		CHECK(r.contains(r));
		CHECK(r.contains(RectangleI(10, 30, 5, 5)));
		CHECK(r.contains(RectangleI(35, 30, 5, 5)));
		CHECK(r.contains(RectangleI(20, 20, 5, 5)));
		CHECK(r.contains(RectangleI(20, 55, 5, 5)));

		CHECK_FALSE(r.contains(RectangleI(9, 30, 5, 5)));
		CHECK_FALSE(r.contains(RectangleI(36, 30, 5, 5)));
		CHECK_FALSE(r.contains(RectangleI(20, 19, 5, 5)));
		CHECK_FALSE(r.contains(RectangleI(20, 56, 5, 5)));
		CHECK_FALSE(r.contains(RectangleI(0, 0, 100, 100)));
	}

	TEST_CASE("offset and set_position move it and keep its size")
	{
		RectangleI r(10, 20, 30, 40);

		r.offset(-5, 7);
		CHECK(r == RectangleI(5, 27, 30, 40));
		r.offset(Vector2I(1, -2));
		CHECK(r == RectangleI(6, 25, 30, 40));
		r.set_position(Vector2I(-3, 0));
		CHECK(r == RectangleI(-3, 0, 30, 40));
	}

	TEST_CASE("set_size keeps the top left")
	{
		RectangleI r(10, 20, 30, 40);

		r.set_size(Vector2I(1, 2));
		CHECK(r == RectangleI(10, 20, 1, 2));
		CHECK(r.bottom_right() == Vector2I(11, 22));
	}

	TEST_CASE("equality compares all four fields")
	{
		const RectangleI r(1, 2, 3, 4);

		CHECK(r == RectangleI(1, 2, 3, 4));
		CHECK(r != RectangleI(0, 2, 3, 4));
		CHECK(r != RectangleI(1, 0, 3, 4));
		CHECK(r != RectangleI(1, 2, 0, 4));
		CHECK(r != RectangleI(1, 2, 3, 0));
		CHECK(RectangleI() == RectangleI::zero);
	}
}

#include <doctest/doctest.h>

#include "engine/math/matrix3x2f.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"

using mattmath::Matrix3x2F;
using mattmath::RectangleF;
using mattmath::RectangleI;
using mattmath::Vector2F;
using mattmath::Vector2I;

// THE ORDER THIS TRANSLATION UNIT INITIALISES IN IS THE FINDING. These are
// namespace-scope objects in a test executable, copied from constants that
// live in a static library, and on this toolchain the executable's own
// objects initialise before the library's do. While the constants were
// dynamically initialised - out-of-line constructors, so not constant
// expressions - every copy below read (0, 0) and nothing said so
// (docs/review/gpt6/README.md, G6-03). A client's own direction table is
// this shape, and so is a default argument stored in a static.
//
// The definitions are constinit now, which is a compile error if any of
// them ever stops being a constant expression. This file is the other half:
// the value a stranger reads, from the position a stranger reads it in.
namespace
{
	const Vector2F COPIED_RIGHT = Vector2F::DIRECTION_RIGHT;
	const Vector2F COPIED_UP_RIGHT = Vector2F::DIRECTION_UP_RIGHT;
	const Vector2F COPIED_ONE = Vector2F::ONE;
	const Vector2I COPIED_INT_ZERO = Vector2I::ZERO;
	const RectangleF COPIED_RECT_ZERO = RectangleF::ZERO;
	const RectangleI COPIED_RECT_INT_ZERO = RectangleI::ZERO;
	const Matrix3x2F COPIED_IDENTITY = Matrix3x2F::identity;

	// A derived value too, since that is what a client global usually is:
	// the sum reads two constants and would have been (0, 0) from either.
	const Vector2F COPIED_DOWN_LEFT_SUM =
		Vector2F::DIRECTION_DOWN + Vector2F::DIRECTION_LEFT;
}

TEST_CASE("a namespace-scope copy in another translation unit sees the value")
{
	CHECK(COPIED_RIGHT == Vector2F(1.0f, 0.0f));
	CHECK(COPIED_UP_RIGHT.x == doctest::Approx(0.70710678f));
	CHECK(COPIED_UP_RIGHT.y == doctest::Approx(-0.70710678f));
	CHECK(COPIED_ONE == Vector2F(1.0f, 1.0f));
	CHECK(COPIED_DOWN_LEFT_SUM == Vector2F(-1.0f, 1.0f));

	CHECK(COPIED_INT_ZERO == Vector2I(0, 0));
	CHECK(COPIED_RECT_ZERO.width == 0.0f);
	CHECK(COPIED_RECT_INT_ZERO.width == 0);

	// The two diagonal elements, which a zero matrix has at zero.
	CHECK(COPIED_IDENTITY.m11 == 1.0f);
	CHECK(COPIED_IDENTITY.m22 == 1.0f);
	CHECK(COPIED_IDENTITY == Matrix3x2F::identity);
}

TEST_CASE("the constants are usable in a constant expression's shape")
{
	// Not the constants themselves - a const object of class type is not a
	// constant expression - but the constructors they are built from are,
	// which is what constinit needs and what a client's own constexpr table
	// can now be built from.
	constexpr Vector2F right(1.0f, 0.0f);
	constexpr Vector2I one(1, 1);
	constexpr RectangleF unit(0.0f, 0.0f, 1.0f, 1.0f);
	constexpr RectangleI unit_int(0, 0, 1, 1);
	constexpr Matrix3x2F flip(-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	static_assert(right.x == 1.0f);
	static_assert(one.y == 1);
	static_assert(unit.width == 1.0f);
	static_assert(unit_int.height == 1);
	static_assert(flip.m11 == -1.0f);

	CHECK(right == Vector2F::DIRECTION_RIGHT);
}

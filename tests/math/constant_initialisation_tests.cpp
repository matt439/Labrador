#include <doctest/doctest.h>

#include "engine/math/matrix3x2f.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"

using labrador::Matrix3x2F;
using labrador::RectangleF;
using labrador::RectangleI;
using labrador::Vector2F;
using labrador::Vector2I;

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
	const Vector2F copied_right = Vector2F::direction_right;
	const Vector2F copied_up_right = Vector2F::direction_up_right;
	const Vector2F copied_one = Vector2F::one;
	const Vector2I copied_int_zero = Vector2I::zero;
	const RectangleF copied_rect_zero = RectangleF::zero;
	const RectangleI copied_rect_int_zero = RectangleI::zero;
	const Matrix3x2F copied_identity = Matrix3x2F::identity;

	// A derived value too, since that is what a client global usually is:
	// the sum reads two constants and would have been (0, 0) from either.
	const Vector2F copied_down_left_sum =
		Vector2F::direction_down + Vector2F::direction_left;
}

TEST_CASE("a namespace-scope copy in another translation unit sees the value")
{
	CHECK(copied_right == Vector2F(1.0f, 0.0f));
	CHECK(copied_up_right.x == doctest::Approx(0.70710678f));
	CHECK(copied_up_right.y == doctest::Approx(-0.70710678f));
	CHECK(copied_one == Vector2F(1.0f, 1.0f));
	CHECK(copied_down_left_sum == Vector2F(-1.0f, 1.0f));

	CHECK(copied_int_zero == Vector2I(0, 0));
	CHECK(copied_rect_zero.width == 0.0f);
	CHECK(copied_rect_int_zero.width == 0);

	// The two diagonal elements, which a zero matrix has at zero.
	CHECK(copied_identity.m11 == 1.0f);
	CHECK(copied_identity.m22 == 1.0f);
	CHECK(copied_identity == Matrix3x2F::identity);
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

	CHECK(right == Vector2F::direction_right);
}

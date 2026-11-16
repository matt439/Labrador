#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/math/vector2i.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace labrador;

TEST_SUITE("Vector2I")
{
	TEST_CASE("converting from Vector2F truncates each component toward zero")
	{
		CHECK(Vector2I(Vector2F(2.9f, -2.9f)) == Vector2I(2, -2));
		CHECK(Vector2I(Vector2F(0.5f, -0.5f)) == Vector2I(0, 0));
		CHECK(Vector2I(Vector2F(-1.0f, 3.0f)) == Vector2I(-1, 3));
	}

	TEST_CASE("float conversion accepts the representable boundary values")
	{
		const float lower = static_cast<float>(std::numeric_limits<int>::min());
		const float upper = std::nextafter(
			static_cast<float>(std::numeric_limits<int>::max()), 0.0f);
		CHECK(Vector2I(Vector2F(lower, upper)) ==
			Vector2I(std::numeric_limits<int>::min(), static_cast<int>(upper)));
		CHECK(Vector2I(Vector2F(-0.0f, -0.999f)) == Vector2I::zero);
	}

	TEST_CASE("float conversion refuses each non-finite component")
	{
		const std::array<float, 3> invalid = {
			std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity()
		};
		for (const float value : invalid)
		{
			CHECK_THROWS_AS(Vector2I(Vector2F(value, 1.0f)), std::invalid_argument);
			CHECK_THROWS_AS(Vector2I(Vector2F(1.0f, value)), std::invalid_argument);
		}
	}

	TEST_CASE("float conversion refuses each out-of-range component")
	{
		const std::array<float, 4> invalid = {
			static_cast<float>(std::numeric_limits<int>::max()),
			std::nextafter(static_cast<float>(std::numeric_limits<int>::min()),
				-std::numeric_limits<float>::infinity()),
			std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()
		};
		for (const float value : invalid)
		{
			CHECK_THROWS_AS(Vector2I(Vector2F(value, 1.0f)), std::out_of_range);
			CHECK_THROWS_AS(Vector2I(Vector2F(1.0f, value)), std::out_of_range);
			Vector2I target(8, 9);
			CHECK_THROWS_AS(target = Vector2I(Vector2F(1.0f, value)), std::out_of_range);
			CHECK(target == Vector2I(8, 9));
		}
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
		Vector2I by_self(2, -3);
		by_self /= by_self;
		CHECK(by_self == Vector2I(1, 1));
	}

	TEST_CASE("zero divisors refuse every division form without partial mutation")
	{
		const Vector2I initial(8, 9);
		Vector2I target = initial;
		CHECK_THROWS_AS(target /= 0, std::invalid_argument);
		CHECK(target == initial);
		CHECK_THROWS_AS(initial / 0, std::invalid_argument);
		for (const Vector2I divisor : {Vector2I(0, 3), Vector2I(2, 0), Vector2I(0, 0)})
		{
			CHECK_THROWS_AS(target /= divisor, std::invalid_argument);
			CHECK(target == initial);
			CHECK_THROWS_AS(initial / divisor, std::invalid_argument);
		}
		Vector2I zero;
		CHECK_THROWS_AS(zero /= zero, std::invalid_argument);
		CHECK(zero == Vector2I::zero);
	}

	TEST_CASE("division overflow refuses every component without partial mutation")
	{
		const int minimum = std::numeric_limits<int>::min();
		for (const Vector2I initial : {Vector2I(minimum, 9), Vector2I(8, minimum)})
		{
			Vector2I target = initial;
			CHECK_THROWS_AS(target /= -1, std::overflow_error);
			CHECK(target == initial);
			CHECK_THROWS_AS(initial / -1, std::overflow_error);
			CHECK_THROWS_AS(target /= Vector2I(-1, -1), std::overflow_error);
			CHECK(target == initial);
			CHECK_THROWS_AS(initial / Vector2I(-1, -1), std::overflow_error);
		}
		CHECK(Vector2I(minimum, minimum) / 1 == Vector2I(minimum, minimum));
		CHECK(Vector2I(minimum, minimum) / minimum == Vector2I(1, 1));
		CHECK(Vector2I(std::numeric_limits<int>::max(), -1) / -1 ==
			Vector2I(-std::numeric_limits<int>::max(), 1));
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

	TEST_CASE("float constructors accept representable boundary fields")
	{
		const float lower = static_cast<float>(std::numeric_limits<int>::min());
		const float upper = std::nextafter(
			static_cast<float>(std::numeric_limits<int>::max()), 0.0f);
		const RectangleI expected(std::numeric_limits<int>::min(), static_cast<int>(upper),
			static_cast<int>(upper), std::numeric_limits<int>::min());
		CHECK(RectangleI(RectangleF(lower, upper, upper, lower)) == expected);
		CHECK(RectangleI(Vector2F(lower, upper), Vector2F(upper, lower)) == expected);
	}

	TEST_CASE("both float constructors refuse invalid input in every field")
	{
		const std::array<float, 3> nonfinite = {
			std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity()
		};
		const std::array<float, 2> outside = {
			static_cast<float>(std::numeric_limits<int>::max()),
			std::nextafter(static_cast<float>(std::numeric_limits<int>::min()),
				-std::numeric_limits<float>::infinity())
		};
		for (std::size_t field = 0; field < 4; ++field)
		{
			for (const float value : nonfinite)
			{
				std::array<float, 4> fields = {1.0f, 2.0f, 3.0f, 4.0f};
				fields[field] = value;
				CHECK_THROWS_AS(RectangleI(RectangleF(fields[0], fields[1], fields[2], fields[3])),
					std::invalid_argument);
				CHECK_THROWS_AS(RectangleI(Vector2F(fields[0], fields[1]),
					Vector2F(fields[2], fields[3])), std::invalid_argument);
			}
			for (const float value : outside)
			{
				std::array<float, 4> fields = {1.0f, 2.0f, 3.0f, 4.0f};
				fields[field] = value;
				CHECK_THROWS_AS(RectangleI(RectangleF(fields[0], fields[1], fields[2], fields[3])),
					std::out_of_range);
				CHECK_THROWS_AS(RectangleI(Vector2F(fields[0], fields[1]),
					Vector2F(fields[2], fields[3])), std::out_of_range);
			}
		}
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

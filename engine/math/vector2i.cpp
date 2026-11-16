#include "engine/math/vector2i.h"

#include "engine/math/vector2f.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace labrador
{
	namespace
	{
		int truncate_to_int(float value)
		{
			if (!std::isfinite(value))
				throw std::invalid_argument("Integer geometry requires finite coordinates");
			const double truncated = std::trunc(static_cast<double>(value));
			if (truncated < static_cast<double>(std::numeric_limits<int>::min())
				|| truncated > static_cast<double>(std::numeric_limits<int>::max()))
				throw std::out_of_range("Integer geometry coordinate is outside the int range");
			return static_cast<int>(truncated);
		}

		void require_division(int numerator, int denominator)
		{
			if (denominator == 0)
				throw std::invalid_argument("Vector2I division requires a nonzero divisor");
			if (numerator == std::numeric_limits<int>::min() && denominator == -1)
				throw std::overflow_error("Vector2I division result is outside the int range");
		}
	}

	Vector2I::Vector2I(const Vector2F& vector)
	{
		this->x = truncate_to_int(vector.x);
		this->y = truncate_to_int(vector.y);
	}
	bool Vector2I::operator==(const Vector2I& other) const
	{
		return this->x == other.x && this->y == other.y;
	}
	bool Vector2I::operator!=(const Vector2I& other) const
	{
		return !(*this == other);
	}
	Vector2I& Vector2I::operator+=(const Vector2I& other)
	{
		this->x += other.x;
		this->y += other.y;
		return *this;
	}
	Vector2I& Vector2I::operator-=(const Vector2I& other)
	{
		this->x -= other.x;
		this->y -= other.y;
		return *this;
	}
	Vector2I& Vector2I::operator*=(const Vector2I& other)
	{
		this->x *= other.x;
		this->y *= other.y;
		return *this;
	}
	Vector2I& Vector2I::operator/=(const Vector2I& other)
	{
		require_division(this->x, other.x);
		require_division(this->y, other.y);
		this->x /= other.x;
		this->y /= other.y;
		return *this;
	}
	Vector2I& Vector2I::operator*=(int other)
	{
		this->x *= other;
		this->y *= other;
		return *this;
	}
	Vector2I& Vector2I::operator/=(int other)
	{
		require_division(this->x, other);
		require_division(this->y, other);
		this->x /= other;
		this->y /= other;
		return *this;
	}
	void Vector2I::offset(int horizontal_amount, int vertical_amount)
	{
		this->x += horizontal_amount;
		this->y += vertical_amount;
	}
	void Vector2I::scale(int horizontal_amount, int vertical_amount)
	{
		this->x *= horizontal_amount;
		this->y *= vertical_amount;
	}
	void Vector2I::set(int new_x, int new_y)
	{
		this->x = new_x;
		this->y = new_y;
	}
	constinit const Vector2I Vector2I::zero = { 0, 0 };
	Vector2I operator+ (const Vector2I& a, const Vector2I& b)
	{
		return Vector2I(a.x + b.x, a.y + b.y);
	}
	Vector2I operator- (const Vector2I& a, const Vector2I& b)
	{
		return Vector2I(a.x - b.x, a.y - b.y);
	}
	Vector2I operator* (const Vector2I& a, const Vector2I& b)
	{
		return Vector2I(a.x * b.x, a.y * b.y);
	}
	Vector2I operator* (const Vector2I& a, int scale)
	{
		return Vector2I(a.x * scale, a.y * scale);
	}
	Vector2I operator/ (const Vector2I& a, const Vector2I& b)
	{
		Vector2I result = a;
		result /= b;
		return result;
	}
	Vector2I operator/ (const Vector2I& a, int scale)
	{
		Vector2I result = a;
		result /= scale;
		return result;
	}
	Vector2I operator* (int scale, const Vector2I& a)
	{
		return Vector2I(a.x * scale, a.y * scale);
	}

}

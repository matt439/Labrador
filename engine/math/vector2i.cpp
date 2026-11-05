#include "engine/math/vector2i.h"

#include "engine/math/vector2f.h"

namespace labrador
{

	Vector2I::Vector2I(const Vector2F& vector)
	{
		this->x = static_cast<int>(vector.x);
		this->y = static_cast<int>(vector.y);
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
		return Vector2I(a.x / b.x, a.y / b.y);
	}
	Vector2I operator/ (const Vector2I& a, int scale)
	{
		return Vector2I(a.x / scale, a.y / scale);
	}
	Vector2I operator* (int scale, const Vector2I& a)
	{
		return Vector2I(a.x * scale, a.y * scale);
	}

}

#include "engine/math/vector2i.h"

#include "engine/math/vector2f.h"

namespace mattmath
{

	Vector2I::Vector2I(int x, int y)
	{
		this->x = x;
		this->y = y;
	}
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
	const Vector2I Vector2I::ZERO = { 0, 0 };
	Vector2I mattmath::operator+ (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x + V2.x, V1.y + V2.y);
	}
	Vector2I mattmath::operator- (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x - V2.x, V1.y - V2.y);
	}
	Vector2I mattmath::operator* (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x * V2.x, V1.y * V2.y);
	}
	Vector2I mattmath::operator* (const Vector2I& V, int S)
	{
		return Vector2I(V.x * S, V.y * S);
	}
	Vector2I mattmath::operator/ (const Vector2I& V1, const Vector2I& V2)
	{
		return Vector2I(V1.x / V2.x, V1.y / V2.y);
	}
	Vector2I mattmath::operator/ (const Vector2I& V, int S)
	{
		return Vector2I(V.x / S, V.y / S);
	}
	Vector2I mattmath::operator* (int S, const Vector2I& V)
	{
		return Vector2I(V.x * S, V.y * S);
	}

}

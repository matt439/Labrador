#include "engine/math/vector2f.h"

#include "engine/math/vector2i.h"
#include "engine/math/scalar.h"

#include <cmath>
#include <algorithm>

namespace mattmath
{

	Vector2F::Vector2F(float f)
	{
		this->x = f;
		this->y = f;
	}
	Vector2F::Vector2F(float x, float y)
	{
		this->x = x;
		this->y = y;
	}
	Vector2F::Vector2F(const Vector2I& vector)
	{
		this->x = static_cast<float>(vector.x);
		this->y = static_cast<float>(vector.y);
	}
	bool Vector2F::operator==(const Vector2F& other) const
	{
		return this->x == other.x && this->y == other.y;
	}
	bool Vector2F::operator!=(const Vector2F& other) const
	{
		return !(*this == other);
	}
	Vector2F& Vector2F::operator+=(const Vector2F& other)
	{
		this->x += other.x;
		this->y += other.y;
		return *this;
	}
	Vector2F& Vector2F::operator-=(const Vector2F& other)
	{
		this->x -= other.x;
		this->y -= other.y;
		return *this;
	}
	Vector2F& Vector2F::operator*=(const Vector2F& other)
	{
		this->x *= other.x;
		this->y *= other.y;
		return *this;
	}
	Vector2F& Vector2F::operator/=(const Vector2F& other)
	{
		this->x /= other.x;
		this->y /= other.y;
		return *this;
	}
	Vector2F& Vector2F::operator*=(float other)
	{
		this->x *= other;
		this->y *= other;
		return *this;
	}
	Vector2F& Vector2F::operator/=(float other)
	{
		this->x /= other;
		this->y /= other;
		return *this;
	}
	float Vector2F::length() const
	{
		return std::sqrtf(this->x * this->x + this->y * this->y);
	}
	float Vector2F::length_squared() const
	{
		return this->x * this->x + this->y * this->y;
	}
	float Vector2F::dot(const Vector2F& other) const
	{
		return this->x * other.x + this->y * other.y;
	}
	Vector2F Vector2F::normalized() const
	{
		float length = this->length();
		if (length == 0.0f)
		{
			return Vector2F::ZERO;
		}
		return Vector2F(this->x / length, this->y / length);
	}
	void Vector2F::normalize()
	{
		float length = this->length();
		if (length == 0.0f)
		{
			this->x = 0.0f;
			this->y = 0.0f;
			return;
		}
		this->x /= length;
		this->y /= length;
	}
	void Vector2F::clamp(const Vector2F& min, const Vector2F& max)
	{
		this->x = std::min(std::max(this->x, min.x), max.x);
		this->y = std::min(std::max(this->y, min.y), max.y);
	}
	Vector2F Vector2F::clamped(const Vector2F& min, const Vector2F& max) const
	{
		return Vector2F(std::min(std::max(this->x, min.x), max.x),
			std::min(std::max(this->y, min.y), max.y));
	}
	float Vector2F::angle() const
	{
		return std::atan2(this->y, this->x);
	}
	float Vector2F::angle(const Vector2F& vec)
	{
		return std::atan2(vec.y, vec.x);
	}
	void Vector2F::to_unit_vector()
	{
		float length = this->length();
		if (length == 0.0f)
		{
			this->x = 1.0f;
			this->y = 0.0f;
		}
		else
		{
			this->x /= length;
			this->y /= length;
		}
	}
	float Vector2F::angle_between(const Vector2F& a, const Vector2F& b)
	{
		const float lengths = a.length() * b.length();
		if (lengths == 0.0f)
		{
			// A zero-length vector points nowhere, so there is no angle to
			// report. Zero is the same answer normalized() gives for the same
			// reason, and it beats dividing by nothing and returning NaN,
			// because NaN travels. Triangle::angle_0/1/2 feed
			// TriangleRightAxisAligned::find_hypotenuse, where every
			// are_equal(NaN, PI_OVER_2) is false, so find_hypotenuse answers
			// -1 and hypotenuse() throws "Triangle is not a right triangle" -
			// about a triangle that is one, two call levels from the actual
			// fault.
			return 0.0f;
		}

		// Mathematically this quotient is in [-1, 1]; computationally it is
		// not. Rounding in the dot product and in two square roots can put it
		// a few ulps outside, and acos of 1.0000001 is NaN - a domain error
		// produced by arithmetic that was never wrong by more than a rounding
		// step (11.1, p.428).
		const float cosine = mattmath::clamp(
			Vector2F::dot(a, b) / lengths, -1.0f, 1.0f);

		return std::acos(cosine);
	}
	Vector2F Vector2F::lerp(const Vector2F& a, const Vector2F& b, float t)
	{
		return Vector2F(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
	}
	Vector2F Vector2F::lerp(const Vector2F& a, const Vector2F& b, const Vector2F& t)
	{
		return Vector2F(a.x + (b.x - a.x) * t.x, a.y + (b.y - a.y) * t.y);
	}
	float Vector2F::distance(const Vector2F& a, const Vector2F& b)
	{
		return sqrtf((a.x - b.x) * (a.x - b.x) +
			(a.y - b.y) * (a.y - b.y));
	}
	float Vector2F::distance_squared(const Vector2F& a, const Vector2F& b)
	{
		return (a.x - b.x) * (a.x - b.x) +
			(a.y - b.y) * (a.y - b.y);
	}
	float Vector2F::dot(const Vector2F& a, const Vector2F& b)
	{
		return a.x * b.x + a.y * b.y;
	}
	float Vector2F::cross(const Vector2F& a, const Vector2F& b)
	{
		return a.x * b.y - a.y * b.x;
	}
	Vector2F Vector2F::unit_vec_from_angle(float angle)
	{
		return Vector2F(std::cos(angle), std::sin(angle));
	}
	Vector2F Vector2F::rotate_vector(const Vector2F& vec, float angle)
	{
		const float cos_angle = std::cos(angle);
		const float sin_angle = std::sin(angle);

		return Vector2F(vec.x * cos_angle - vec.y * sin_angle,
			vec.x * sin_angle + vec.y * cos_angle);
	}
	Vector2F Vector2F::unit_vector(const Vector2F& vec)
	{
		float length = vec.length();
		if (length == 0.0f)
		{
			return Vector2F(1.0f, 0.0f);
		}
		else
		{
			return Vector2F(vec.x / length, vec.y / length);
		}
	}

	Vector2F Vector2F::normal(const Vector2F& vec)
	{
		return Vector2F(-vec.y, vec.x);
	}

	const Vector2F Vector2F::ZERO = { 0.0f, 0.0f };
	const Vector2F Vector2F::ONE = { 1.0f, 1.0f };
	const Vector2F Vector2F::DIRECTION_RIGHT = { 1.0f, 0.0f };
	const Vector2F Vector2F::DIRECTION_DOWN = { 0.0f, 1.0f };
	const Vector2F Vector2F::DIRECTION_LEFT = { -1.0f, 0.0f };
	const Vector2F Vector2F::DIRECTION_UP = { 0.0f, -1.0f };
	// Written out rather than derived. unit_vector(DIRECTION_UP +
	// DIRECTION_RIGHT) and so on is a function call and a square root per
	// constant during dynamic initialisation, and reads the four cardinal
	// constants above while they are themselves being initialised.
	//
	// Inside this file that ordering is defined - initialisation runs in
	// declaration order within a translation unit - but nothing extends that
	// guarantee across one. Any other TU whose own namespace-scope initialiser
	// named a diagonal would have got (0, 0), silently and depending on link
	// order. The engine has already been bitten by exactly this shape once,
	// with ViewportManager::DIVIDER_COLOUR.
	//
	// The literal is the float nearest 1/sqrt(2), which is the value the
	// square root produced.
	namespace
	{
		constexpr float ROOT_HALF = 0.70710678f;
	}
	const Vector2F Vector2F::DIRECTION_UP_RIGHT = { ROOT_HALF, -ROOT_HALF };
	const Vector2F Vector2F::DIRECTION_DOWN_RIGHT = { ROOT_HALF, ROOT_HALF };
	const Vector2F Vector2F::DIRECTION_DOWN_LEFT = { -ROOT_HALF, ROOT_HALF };
	const Vector2F Vector2F::DIRECTION_UP_LEFT = { -ROOT_HALF, -ROOT_HALF };


	Vector2F mattmath::operator- (const Vector2F& V)
	{
		return Vector2F(-V.x, -V.y);
	}
	Vector2F mattmath::operator+ (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x + V2.x, V1.y + V2.y);
	}
	Vector2F mattmath::operator- (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x - V2.x, V1.y - V2.y);
	}
	Vector2F mattmath::operator* (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x * V2.x, V1.y * V2.y);
	}
	Vector2F mattmath::operator* (const Vector2F& V, float S)
	{
		return Vector2F(V.x * S, V.y * S);
	}
	Vector2F mattmath::operator/ (const Vector2F& V1, const Vector2F& V2)
	{
		return Vector2F(V1.x / V2.x, V1.y / V2.y);
	}
	Vector2F mattmath::operator/ (const Vector2F& V, float S)
	{
		return Vector2F(V.x / S, V.y / S);
	}
	Vector2F mattmath::operator* (float S, const Vector2F& V)
	{
		return Vector2F(V.x * S, V.y * S);
	}

}

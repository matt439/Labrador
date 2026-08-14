#include "engine/math/matrix3x2f.h"

#include "engine/math/vector2f.h"

#include <cmath>
#include <stdexcept>

namespace mattmath
{
	// Spelled out rather than defaulted, so the constant states its own value
	// instead of inheriting it from six member initialisers in another file.
	const Matrix3x2F Matrix3x2F::identity = Matrix3x2F(1.0f, 0.0f,
		0.0f, 1.0f,
		0.0f, 0.0f);

	Matrix3x2F::Matrix3x2F(float m11, float m12,
		float m21, float m22,
		float m31, float m32)
	{
		this->m11 = m11;
		this->m12 = m12;
		this->m21 = m21;
		this->m22 = m22;
		this->m31 = m31;
		this->m32 = m32;
	}
	Matrix3x2F Matrix3x2F::translation(const Vector2F& offset)
	{
		return Matrix3x2F(1.0f, 0.0f,
			0.0f, 1.0f,
			offset.x, offset.y);
	}
	Matrix3x2F Matrix3x2F::rotation(float angle)
	{
		const float cosine = std::cos(angle);
		const float sine = std::sin(angle);

		// The first row is where DIRECTION_RIGHT lands, which is what makes
		// this agree with Vector2F::unit_vec_from_angle - see the header.
		return Matrix3x2F(cosine, sine,
			-sine, cosine,
			0.0f, 0.0f);
	}
	Matrix3x2F Matrix3x2F::scaling(const Vector2F& factor)
	{
		return Matrix3x2F(factor.x, 0.0f,
			0.0f, factor.y,
			0.0f, 0.0f);
	}
	Matrix3x2F Matrix3x2F::scaling(float factor)
	{
		return Matrix3x2F::scaling(Vector2F(factor));
	}
	Point2F Matrix3x2F::transform_point(const Point2F& point) const
	{
		return Point2F(point.x * this->m11 + point.y * this->m21 + this->m31,
			point.x * this->m12 + point.y * this->m22 + this->m32);
	}
	Vector2F Matrix3x2F::transform_vector(const Vector2F& vector) const
	{
		return Vector2F(vector.x * this->m11 + vector.y * this->m21,
			vector.x * this->m12 + vector.y * this->m22);
	}
	Vector2F Matrix3x2F::translation() const
	{
		return Vector2F(this->m31, this->m32);
	}
	float Matrix3x2F::determinant() const
	{
		return this->m11 * this->m22 - this->m12 * this->m21;
	}
	Matrix3x2F Matrix3x2F::inverse() const
	{
		// Named for what it is rather than for the operation that produced it,
		// because `determinant` here would hide the member function of that
		// name from the rest of this body.
		const float area_scale = this->determinant();
		const float inverse_area_scale = 1.0f / area_scale;

		// Both halves are load-bearing. A zero, denormal or NaN determinant
		// fails on the reciprocal; an infinite one produces a finite zero
		// reciprocal and a whole matrix of zeroes, which is a collapse wearing
		// a plausible face. See the header for why no tolerance appears here.
		if (!std::isfinite(area_scale) || !std::isfinite(inverse_area_scale))
		{
			throw std::invalid_argument("Matrix3x2F::inverse: the transform "
				"collapses the plane onto a line or a point and cannot be "
				"undone");
		}

		// The linear part is the 2x2 adjugate over the determinant. The
		// bottom row is the translation taken back through that inverted
		// linear part and negated - undoing the move requires knowing what the
		// rotation and scale did to it, which is why it is not simply -m31.
		return Matrix3x2F(
			this->m22 * inverse_area_scale,
			-this->m12 * inverse_area_scale,
			-this->m21 * inverse_area_scale,
			this->m11 * inverse_area_scale,
			(this->m21 * this->m32 - this->m22 * this->m31) *
				inverse_area_scale,
			(this->m12 * this->m31 - this->m11 * this->m32) *
				inverse_area_scale);
	}
	bool Matrix3x2F::operator==(const Matrix3x2F& other) const
	{
		return this->m11 == other.m11 && this->m12 == other.m12 &&
			this->m21 == other.m21 && this->m22 == other.m22 &&
			this->m31 == other.m31 && this->m32 == other.m32;
	}
	bool Matrix3x2F::operator!=(const Matrix3x2F& other) const
	{
		return !(*this == other);
	}
	Matrix3x2F& Matrix3x2F::operator*=(const Matrix3x2F& other)
	{
		*this = *this * other;
		return *this;
	}
	Matrix3x2F operator* (const Matrix3x2F& first, const Matrix3x2F& second)
	{
		// The bottom row is the first transform's translation carried through
		// the second's linear part, plus the second's own translation - which
		// is transform_point applied to first.translation(), and is the whole
		// of what the implied (0, 0, 1) column does.
		return Matrix3x2F(
			first.m11 * second.m11 + first.m12 * second.m21,
			first.m11 * second.m12 + first.m12 * second.m22,
			first.m21 * second.m11 + first.m22 * second.m21,
			first.m21 * second.m12 + first.m22 * second.m22,
			first.m31 * second.m11 + first.m32 * second.m21 + second.m31,
			first.m31 * second.m12 + first.m32 * second.m22 + second.m32);
	}
}

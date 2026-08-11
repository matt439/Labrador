#pragma once

#include "engine/math/scalar.h"

namespace mattmath
{
	struct Vector2I;

	struct Vector2F
	{
		float x = 0.0f;
		float y = 0.0f;

		Vector2F() = default;
		Vector2F(const Vector2F&) = default;
		// explicit, all of them. A float is not a vector, a Vector2I is not a
		// Vector2F, and a RectangleF is not a Quad - and while these
		// conversions were implicit the compiler inserted them silently at
		// /W4 /WX. Two of the Quad forms end in a throw, so a wrong-typed
		// argument became a runtime exception from inside the callee with no
		// conversion visible at the call site.
		explicit Vector2F(float f);
		Vector2F(float x, float y);
		explicit Vector2F(const mattmath::Vector2I& vector);

		bool operator==(const Vector2F& other) const;
		bool operator!=(const Vector2F& other) const;

		Vector2F& operator+=(const Vector2F& other);
		Vector2F& operator-=(const Vector2F& other);
		Vector2F& operator*=(const Vector2F& other);
		Vector2F& operator/=(const Vector2F& other);
		Vector2F& operator*=(float other);
		Vector2F& operator/=(float other);

		float length() const;
		float length_squared() const;

		float dot(const Vector2F& other) const;

		// Zero-length in, zero-length out. A zero vector has no direction, and
		// returning zero lets the caller detect that; dividing by the length
		// produced NaN, which then propagated silently through velocities and
		// shape validation.
		//
		// Note this is deliberately NOT the same contract as to_unit_vector()
		// and unit_vector(), which substitute (1, 0) for a zero vector. Use
		// those only where an arbitrary direction is acceptable.
		Vector2F normalized() const;
		void normalize();

		void clamp(const Vector2F& min, const Vector2F& max);
		Vector2F clamped(const Vector2F& min, const Vector2F& max) const;

		float angle() const;
		static float angle(const Vector2F& vec);


		// Substitutes (1, 0) for a zero-length vector - i.e. it invents a
		// direction rather than reporting that there is none. See normalize().
		void to_unit_vector();

		static float angle_between(const Vector2F& a, const Vector2F& b);

		static Vector2F lerp(const Vector2F& a, const Vector2F& b, float t);
		static Vector2F lerp(const Vector2F& a, const Vector2F& b, const Vector2F& t);

		static float distance(const Vector2F& a, const Vector2F& b);
		static float distance_squared(const Vector2F& a, const Vector2F& b);

		static float dot(const Vector2F& a, const Vector2F& b);

		// The 2D cross product: a scalar, not a vector. In three dimensions
		// the cross product of two vectors is the third axis; in two there is
		// no third axis, and what survives is its signed length,
		// a.x * b.y - a.y * b.x.
		//
		// It is the signed area of the parallelogram a and b span, so its
		// magnitude measures how far from parallel they are - the exact
		// counterpart of dot() measuring how far from perpendicular. Its sign
		// is the orientation test: positive when b lies counter-clockwise of
		// a, negative when clockwise, and zero when the two are parallel,
		// which is the only reliable way to ask that question.
		//
		// Comparing three points is this on their differences:
		// cross(b - a, c - a) is twice the signed area of triangle abc, and
		// is positive exactly when abc winds counter-clockwise. That
		// composite is Ericson's ORIENT2D; ericson_math.h spells it
		// signed_2D_tri_area.
		//
		// Beware the y-down screen convention: "counter-clockwise" above is
		// stated in maths axes. On screen, where y grows downward, a positive
		// result reads as clockwise. The sign is consistent either way - it
		// is the word that flips.
		static float cross(const Vector2F& a, const Vector2F& b);

		static Vector2F unit_vec_from_angle(float angle);

		// Returns (1, 0) for a zero-length vector. See to_unit_vector().
		static Vector2F unit_vector(const Vector2F& vec);
		
		static Vector2F normal(const Vector2F& vec);

		static const Vector2F ZERO;
		static const Vector2F ONE;
		static const Vector2F DIRECTION_RIGHT;
		static const Vector2F DIRECTION_DOWN;
		static const Vector2F DIRECTION_LEFT;
		static const Vector2F DIRECTION_UP;
		static const Vector2F DIRECTION_UP_RIGHT;
		static const Vector2F DIRECTION_DOWN_RIGHT;
		static const Vector2F DIRECTION_DOWN_LEFT;
		static const Vector2F DIRECTION_UP_LEFT;
	};

	// A point and a vector are the same two floats, and the library has always
	// said so. The name is here rather than beside a forward declaration
	// because this is the type it names.
	typedef Vector2F Point2F;

	// Reversing a direction. The collision module does this constantly - a
	// contact normal points from the first shape to the second, so the second
	// needs the other one - and every site wrote Vector2F(-v.x, -v.y) or
	// v * -1.0f by hand, in two spellings.
	Vector2F operator- (const Vector2F& V);

	Vector2F operator+ (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator- (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator* (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator* (const Vector2F& V, float S);
	Vector2F operator/ (const Vector2F& V1, const Vector2F& V2);
	Vector2F operator/ (const Vector2F& V, float S);
	Vector2F operator* (float S, const Vector2F& V);
}

#pragma once

#include "engine/math/vector2f.h"

namespace mattmath
{
	// A 2D affine transform - translation, rotation, scale and shear held as
	// one value that composes and inverts. This is what to reach for when
	// moving or turning something by hand: build the transform once, then push
	// points through it.
	//
	// Three rows of two, and the name says so, because the third column of a
	// 2D affine transform is (0, 0, 1) in every transform a 2D game can build.
	// Storing it means storing three constants and then either ignoring them
	// on every point - which is a lie the type invites - or dividing by the
	// third, which buys a perspective projection this engine has ruled out
	// permanently (PHILOSOPHY.md, Non-goals: 3D). Implied, the column cannot
	// be written to and cannot be wrong:
	//
	//     | m11  m12  (0) |
	//     | m21  m22  (0) |
	//     | m31  m32  (1) |
	//
	// The 2x2 block is the linear part - what happens to directions - and the
	// bottom row is the translation. That split is why transform_point and
	// transform_vector are two functions rather than one function and a flag.
	//
	// ROW VECTORS. A point is a row, p' = p * M, and a product therefore reads
	// left to right in the order the transforms happen:
	//
	//     const Matrix3x2F spin_about_centre =
	//         Matrix3x2F::translation(-centre) *   // bring the centre to the
	//         Matrix3x2F::rotation(angle) *        //   origin, turn there,
	//         Matrix3x2F::translation(centre);     //   and put it back.
	//
	//     const Point2F turned = spin_about_centre.transform_point(corner);
	//
	// The other convention - column vectors, p' = M * p - writes that product
	// backwards, and both are correct mathematics. This one is chosen because
	// the order on the page is the order in time, and the order is the whole
	// of what a reader has to get right (T4). Composition is associative and
	// is NOT commutative: rotate-then-move and move-then-rotate are different
	// transforms, exactly as they are different actions.
	//
	// This is deliberately not the general 3x3 matrix that used to live in
	// MattMath. That one had a dynamically sized base that never sized itself,
	// so every factory wrote nine elements out of bounds; it had no tests and
	// no callers, and round-2 review deleted it (docs/review/all-findings.md,
	// findings 9, 39 and 40). What is here instead is the six floats a 2D game
	// actually applies, with the contract stated and the behaviour pinned.
	struct Matrix3x2F
	{
		// The identity, by default: a transform that does nothing is the one
		// worth getting for free, and it is the value a composition starts
		// from.
		float m11 = 1.0f, m12 = 0.0f;
		float m21 = 0.0f, m22 = 1.0f;
		float m31 = 0.0f, m32 = 0.0f;

		Matrix3x2F() = default;
		Matrix3x2F(const Matrix3x2F&) = default;
		Matrix3x2F(float m11, float m12,
			float m21, float m22,
			float m31, float m32);

		// Moving. The offset is where the origin ends up.
		static Matrix3x2F translation(const Vector2F& offset);

		// Turning, about the origin, in radians - to turn about anything else,
		// compose, as the worked example above does.
		//
		// The sense is the library's own, and the two other places that name
		// an angle agree with it exactly:
		// rotation(a).transform_vector(Vector2F::DIRECTION_RIGHT) is
		// Vector2F::unit_vec_from_angle(a), and Vector2F::angle() reads that
		// angle back.
		//
		// Which way it looks on screen is the y-down caveat Vector2F::cross
		// states at length. In maths axes a positive angle turns
		// counter-clockwise; on a screen where y grows downward the same
		// number turns the picture clockwise. The arithmetic is consistent
		// either way - it is the word that flips.
		static Matrix3x2F rotation(float angle);

		// Resizing, about the origin. A negative factor is a mirror, and a
		// zero factor is legal and collapses the plane - see inverse(), which
		// is where that stops being free.
		static Matrix3x2F scaling(const Vector2F& factor);
		static Matrix3x2F scaling(float factor);

		// Through the whole transform, translation included. For a position.
		Point2F transform_point(const Point2F& point) const;

		// Through the linear part only, so the translation is skipped. For a
		// direction, a velocity, an offset, an extent - anything that measures
		// a difference between two positions rather than naming one, and which
		// moving the world therefore must not change.
		//
		// Two named functions rather than one and a flag, and no operator*
		// spelling for either: Point2F IS Vector2F - vector2f.h says so and
		// means it - so nothing in the type system can tell a position from a
		// direction, and a `p * m` would have to guess which was meant. Naming
		// the two cases makes the call site say it.
		//
		// A NORMAL is the trap worth naming, because it looks like a direction
		// and does not behave like one here. A normal survives this only under
		// a rotation and a uniform scale. Under a non-uniform scale or a
		// shear, the perpendicular of a transformed edge is not the transform
		// of the edge's perpendicular - the correct object is the inverse
		// transpose, which this type does not offer. Transform the edge and
		// take the normal of the result.
		Vector2F transform_vector(const Vector2F& vector) const;

		// Where the origin lands: the bottom row, read back. The static
		// overload above builds a transform out of an offset; this reports the
		// offset a composed transform ended up with. Vector2F::angle() is the
		// same pairing of a factory and a reader under one name.
		Vector2F translation() const;

		// The signed area scale of the linear part - how much larger this
		// transform makes things, negative when it also mirrors them. Zero
		// means the transform has collapsed the plane onto a line or a point,
		// which is exactly the case that cannot be undone.
		float determinant() const;

		// The transform that undoes this one: composing the two in either
		// order gives the identity, to float rounding.
		//
		// Throws std::invalid_argument when there is no such transform. A
		// collapsed transform has no inverse, and the alternative to throwing
		// is a matrix full of infinities that poisons silently every point
		// taken through it and names nothing when it does (T6).
		//
		// The test is that the reciprocal of the determinant is finite, and it
		// is deliberately not a comparison against EPSILON. Every tolerance in
		// scalar.h's ordering bounds a LENGTH, where an absolute number means
		// something over a bounded range of coordinates. A determinant is an
		// AREA RATIO, so the same bound would mean something different at
		// every scale: a uniform scale of 0.005 - a legitimate, invertible,
		// zoomed-far-out camera - has a determinant of 2.5e-5, and EPSILON
		// would refuse it as collapsed. Only the values that cannot produce a
		// finite answer are refused.
		Matrix3x2F inverse() const;

		// Exact, element by element, like Vector2F's and for the same reason
		// (/fp:precise, cmake/settings.cmake). Two transforms built by
		// different routes to the same place will usually differ in the last
		// bits and compare unequal; compare their effect on a point with
		// are_equal when that is the question being asked.
		bool operator==(const Matrix3x2F& other) const;
		bool operator!=(const Matrix3x2F& other) const;

		// this, then other. Same order as the free operator below.
		Matrix3x2F& operator*=(const Matrix3x2F& other);

		static const Matrix3x2F identity;
	};

	// The composition: the transform that applies `first` and then `second`.
	Matrix3x2F operator* (const Matrix3x2F& first, const Matrix3x2F& second);

	// NOT HERE: a decomposition back into a position, an angle and a scale.
	// Composition is the whole reason the six floats beat those three numbers
	// - a rotation applied under a non-uniform scale is a shear, and no
	// (position, angle, scale) triple can hold one. A rotation() reader would
	// therefore be right for the transforms that happen to be simple and
	// quietly wrong for the rest, which is precisely the failure this type
	// exists to avoid. Code that wants those three numbers back should keep
	// the three numbers; translation() is offered because the bottom row is
	// stored, not derived, so reading it back cannot lie.
	//
	// NOT HERE: a transform of a RectangleF. The image of an axis-aligned box
	// under a rotation is not an axis-aligned box, so the honest return type
	// is a Quad or a RectangleRotated - and either would make the smallest
	// type in this library depend on two of its largest. The four corners
	// through transform_point is the same work with the result named
	// correctly.
}

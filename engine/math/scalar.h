#pragma once


namespace labrador
{
	struct Vector2F;

	// Engine angles use radians, including Vector2F::unit_vec_from_angle
	// and RectangleRotated::angle.
	constexpr float pi = 3.14159265358979323846f;
	constexpr float pi_over_2 = pi / 2.0f;

	// General absolute comparison tolerance. It does not scale with coordinate
	// magnitude: at 5000, one float step is about 4.883e-4, so adding this
	// tolerance rounds back to 5000. Choose a tolerance for the quantity and
	// coordinate range being compared; increasing this constant changes every
	// caller using its default.
	//
	// Related thresholds serve different purposes:
	// - ericson_math.cpp's segment_parallel_epsilon (1e-6) expands a segment's
	//   slab test near parallel axes.
	// - resolve.cpp's require_unit bound (1e-3) checks squared length and
	//   refuses non-unit input instead of correcting geometry.
	// - resolve.h's min_axis_alignment (0.1) bounds the permitted separation
	//   direction; it is not a rounding tolerance.
	// - RectangleRotated::edges_valid scales this tolerance by max(1, length)
	//   when comparing opposite-edge lengths.
	// - inflate_convex_polygon uses this tolerance in a mitre solve whose
	//   branch affects produced vertices; it is not a collision query.
	constexpr float default_epsilon = 0.0001f;

	// Takes the min branch first, so with a floor above the ceiling a value
	// below the floor gets the floor and any other value gets the ceiling.
	// Callers wanting one bound to win for an inverted range have to say so
	// themselves.
	float clamp(float value, float min, float max);

	// Strict absolute comparison: abs(a - b) < epsilon. The vector overload
	// applies it to both components. A nonpositive epsilon matches nothing;
	// NaN differences do not compare equal.
	bool are_equal(float a, float b, float epsilon = default_epsilon);
	bool are_equal(const Vector2F& a, const Vector2F& b,
		float epsilon = default_epsilon);

	// Linear interpolation without clamping t; values outside [0, 1]
	// extrapolate beyond the endpoints.
	float lerp(float a, float b, float t);
}

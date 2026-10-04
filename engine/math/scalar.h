#pragma once


namespace mattmath
{
	struct Vector2F;

	constexpr float PI = 3.14159265358979323846f;
	constexpr float PI_OVER_2 = PI / 2.0f;

	// The general comparison tolerance, and the one number here that carries a
	// warning.
	//
	// EPSILON is ABSOLUTE, so it only means anything over a bounded range of
	// coordinates, and the range it means something over is smaller than the
	// one a game's world easily reaches. One float ulp at 5000 is 4.883e-4,
	// so `5000.0f + EPSILON == 5000.0f` exactly: five thousand units from the
	// origin, the engine's general tolerance is a fifth of the smallest
	// representable step, which is to say it is exact equality wearing a
	// costume. The tests mostly run within a few hundred units of the origin,
	// where one ulp is 6.1e-5 and EPSILON is a meaningful ~1.6 of them - so
	// the suite exercises the range where this constant works and a game may
	// run in the range where it does not.
	//
	// That is not a bug, and the reason is stated so nobody "fixes" it by
	// making the number bigger. Every use of EPSILON on the collision path
	// CLASSIFIES - is this vector degenerate, is this axis usable - and none
	// of them MEASURE. resolve.cpp's guard, the one place a tolerance could
	// move geometry, refuses rather than returns a number (see
	// MIN_AXIS_ALIGNMENT). tests/collision/narrow_phase_tests.cpp runs the
	// separation sweep translated out to 600,000 units and it holds exactly,
	// so the analytic path needs no tolerance at all at world scale.
	//
	// Off that path there is exactly one exception, and it is named here so
	// the sentence above can stay absolute: inflate_convex_polygon compares a
	// determinant against EPSILON, and that comparison decides how far a
	// produced vertex moves. It is a mitre solve, not a collision query, and
	// no caller in this repository reaches it.
	//
	// The ordering, which is what Ericson insists a set of tolerances has
	// (8.4.3, p.377 - a query tolerance must exceed the tolerance geometry was
	// built with, or a primitive placed on one side is missed by a strict
	// query). Smallest first:
	//
	//   SEGMENT_PARALLEL_EPSILON  1e-6  ericson_math.cpp, anonymous namespace.
	//                                   A slab-test fudge for a segment nearly
	//                                   parallel to an axis. Two orders tighter
	//                                   than EPSILON on purpose; they are not
	//                                   the same quantity.
	//   EPSILON                   1e-4  this constant. Classification only.
	//   require_unit's bound      1e-3  resolve.cpp, on a SQUARED length, so
	//                                   it is twice as tight as it looks
	//                                   against a length. Classification: it
	//                                   catches a vector that was never
	//                                   normalised, and refuses rather than
	//                                   correcting.
	//   MIN_AXIS_ALIGNMENT        0.1   resolve.h. Not a rounding tolerance at
	//                                   all - a bound on how oblique an axis
	//                                   may be before separating along it is a
	//                                   category error. Named here because it
	//                                   is the one that decides whether a
	//                                   caller gets an answer.
	//
	// One member of the family cannot be ranked in that order at all, and is
	// listed apart rather than pretended into it: RectangleRotated::edges_valid
	// compares lengths with EPSILON * max(1, length), a RELATIVE tolerance. It
	// has to be relative, because an absolute one rejects large rectangles
	// that are perfectly square - which is the same arithmetic this whole note
	// is about, met from the other side.
	//
	// Anything added to this set states which of those three jobs it does, and
	// where it sits in the order. A tolerance that may move geometry has to be
	// strictly larger than one that may only classify it.
	constexpr float EPSILON = 0.0001f;

	// Takes the min branch first, so with a floor above the ceiling a value
	// below the floor gets the floor and any other value gets the ceiling.
	// Callers wanting one bound to win for an inverted range have to say so
	// themselves - camera_tools.cpp does, and writes the four lines out.
	float clamp(float value, float min, float max);

	bool are_equal(float a, float b, float epsilon = EPSILON);
	bool are_equal(const mattmath::Vector2F& a, const mattmath::Vector2F& b,
		float epsilon = EPSILON);

	// NOT HERE: to_radians and to_degrees. Every angle in the engine is
	// radians, because that is what <cmath> takes and what
	// Vector2F::unit_vec_from_angle and RectangleRotated::angle answer in, so
	// a pair of conversions would be a unit system nobody uses (T1).

	float lerp(float a, float b, float t);
}

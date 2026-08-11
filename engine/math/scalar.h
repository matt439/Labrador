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
	// one this game runs at. One float ulp at 5000 is 4.883e-4, so
	// `5000.0f + EPSILON == 5000.0f` exactly: out where the levels are, the
	// engine's general tolerance is a fifth of the smallest representable
	// step, which is to say it is exact equality wearing a costume. The tests
	// mostly run within a few hundred units of the origin, where one ulp is
	// 6.1e-5 and EPSILON is a meaningful ~1.6 of them - so the suite exercises
	// the range where this constant works and the game runs in the range where
	// it does not.
	//
	// That is not currently a bug, and the reason is worth recording so nobody
	// "fixes" it by making the number bigger. Every use of EPSILON on the
	// collision path CLASSIFIES - is this vector degenerate, is this axis
	// usable - and none of them MEASURE. The one place a tolerance would have
	// moved geometry was resolve.cpp's guard, and that now refuses rather than
	// returns a number (see MIN_AXIS_ALIGNMENT). The separation sweep was
	// re-run translated out to 600,000 units and holds exactly, so the
	// analytic path needs no tolerance at all at world scale.
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
	//                                   than EPSILON on purpose; they were
	//                                   never the same quantity.
	//   EPSILON                   1e-4  this constant. Classification only.
	//   MIN_EDGE_LENGTH           1e-3  narrow_phase.cpp. Classification: an
	//                                   edge shorter than this contributes no
	//                                   separating axis, because normalising
	//                                   it would amplify its rounding error
	//                                   into a direction. Below anything the
	//                                   content contains.
	//   require_unit's bound      1e-3  resolve.cpp, on a SQUARED length, so
	//                                   it is twice as loose as it looks
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
	// has to be relative, because an absolute one rejected large rectangles
	// that were perfectly square - which is the same arithmetic this whole note
	// is about, met from the other side.
	//
	// Anything added to this set states which of those three jobs it does, and
	// where it sits in the order. A tolerance that may move geometry has to be
	// strictly larger than one that may only classify it.
	constexpr float EPSILON = 0.0001f;

	// Takes the min branch first, so with a floor above the ceiling the
	// ceiling is never consulted and the floor wins. Callers wanting the other
	// answer for an inverted range have to say so themselves - camera_tools.cpp
	// is the one place that needed to, and it writes the four lines out.
	//
	// clamp_ref, the in-place form of each, is gone: it had no caller in the
	// tree and could not have acquired a useful one, for the reason above.
	// The int overload went the same way, and for the plainer reason: every
	// clamp in either repository is a float one.
	float clamp(float value, float min, float max);

	bool are_equal(float a, float b, float epsilon = EPSILON);
	bool are_equal(const mattmath::Vector2F& a, const mattmath::Vector2F& b,
		float epsilon = EPSILON);

	// NOT HERE: to_radians and to_degrees. Both had zero callers in this
	// repository and zero in the client - every angle in the engine is
	// already radians, because that is what <cmath> takes and what
	// Vector2F::unit_vec_from_angle and RectangleRotated::angle answer in.
	// A pair of conversions with no caller is a unit system nobody uses (T1).

	float lerp(float a, float b, float t);
}

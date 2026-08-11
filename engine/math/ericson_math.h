#pragma once

// Real-Time Collision Detection
// Christer Ericson
// 2005

#include "engine/math/circle.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include <span>

namespace mattmath
{
	// Routines ported from the book, in 2D, keeping its names and its comments
	// where they still apply.
	//
	// The originals are 3D and most of them lose a dimension mechanically: an
	// AABB drops a slab, a sphere becomes a circle, a triangle stays a triangle.
	// Where a routine does not translate it is absent rather than half-ported,
	// and where the 2D form differs in substance - the separating-axis set,
	// which is complete in 2D and is not in 3D - the difference is documented
	// at the point of use. See engine/collision/narrow_phase.h.
	//
	// The book's own Vector, Point, AABB, Sphere and OBB declarations used to
	// sit here commented out, next to a list of its function signatures as
	// printed. They are gone: mattmath supplies every one of those types, the
	// declarations below are the ported forms, and a commented-out parallel
	// vocabulary is a second definition waiting to disagree with the first.
	//
	// The file also ended on a bare `// 132`, marking the page the original
	// porting effort stopped at. Pages 133 to 551 have since been read - see
	// docs/review/rtcd/ - so the marker meant nothing any more and went with
	// them.

	bool test_AABB_AABB(const mattmath::RectangleF& a,
		const mattmath::RectangleF& b);

	bool test_circle_circle(const mattmath::Circle& a,
		const mattmath::Circle& b);

	mattmath::Point2F closest_pt_point_triangle(
		const mattmath::Point2F& p,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c);

	bool test_circle_AABB(const mattmath::Circle& s,
		const mattmath::RectangleF& b);

	bool test_circle_AABB(const mattmath::Circle& s,
		const mattmath::RectangleF& b, mattmath::Point2F& p);

	float sq_dist_point_AABB(const mattmath::Point2F& p,
		const mattmath::RectangleF& b);

	void closest_pt_point_AABB(const mattmath::Point2F& p,
		const mattmath::RectangleF& b, mattmath::Point2F& q);

	bool test_circle_triangle(const mattmath::Circle& s,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c, mattmath::Point2F& p);

	// Triangle against AABB (5.2.9) is deliberately absent, not pending. In 3D
	// it is a thirteen-axis separating-axis test worth writing by hand; in 2D
	// the axis set collapses to the box's two normals plus the triangle's
	// three, which is exactly what narrow_phase already runs for any pair of
	// convex polygons. Porting it would add a second implementation of a test
	// the engine has, differing only in that this one could not report a
	// penetration depth. Its commented-out body lived here for a long time and
	// is gone; the decision is the thing worth keeping.


	// Whether the segment p0p1 meets the box, boundary included.
	//
	// Written in the accepting form - it returns the conjunction of three
	// overlaps rather than falling through a run of rejections - so a NaN
	// coordinate in either endpoint or in the box reports no intersection.
	// See test_AABB_AABB, which is the same decision, and the note on EPSILON
	// in scalar.h for where SEGMENT_PARALLEL_EPSILON sits in the ordering.
	bool test_segment_AABB(const mattmath::Point2F& p0,
		const mattmath::Point2F& p1, const mattmath::AABB& b);

	// Twice the signed area of triangle abc. This is the book's ORIENT2D
	// predicate, and it is exactly mattmath::Vector2F::cross(b - a, c - a) -
	// see that function for what the sign means and for the y-down caveat.
	// Written out here in the subtracted form the book uses, which needs no
	// temporaries.
	//
	// Zero means the three points are collinear. Callers wanting only the
	// orientation should compare this against zero, or compare two of these
	// results for sign - never multiply two of them together. Use
	// strictly_opposite_sides below, which is what that comparison is called.
	//
	// The hazard the multiply carries is UNDERFLOW, not the overflow an
	// earlier version of this note claimed. signed_2D_tri_area subtracts
	// before it multiplies, so its result is the size of the triangle rather
	// than the size of the world, and even at 600,000 units the product of two
	// of them stays fifteen orders inside float's range. What it does not
	// survive is the other end: two small opposite-signed areas - a crossing
	// that is nearly a graze - multiply to exactly 0.0f, and a test written as
	// `a * b < 0.0f` then reports no crossing at all. A NaN in either operand
	// does the same.
	float signed_2D_tri_area(const mattmath::Point2F& a,
		const mattmath::Point2F& b, const mattmath::Point2F& c);

	// Whether two signed areas place their points on strictly opposite sides
	// of the line they were both measured against.
	//
	// Strict on purpose: a zero is on the line, and "on the line" is not
	// "opposite" to anything. That is what makes it reject the degenerate
	// cases - a repeated vertex, three collinear points - rather than
	// classifying them arbitrarily, and it is why this cannot be written as
	// `(lhs < 0.0f) != (rhs < 0.0f)`, which calls an exact zero "positive".
	bool strictly_opposite_sides(float lhs, float rhs);

	// The signed area enclosed by a polygon given as its vertices in order,
	// closing automatically from the last back to the first. Positive for one
	// winding and negative for the other; zero when the polygon encloses
	// nothing, which includes every degenerate case - fewer than three
	// vertices, repeated vertices, all vertices collinear.
	//
	// This is Newell's method (12.4.2) with two dimensions removed. Newell
	// derives a polygon's normal by summing a term over every edge rather than
	// taking one cross product of two edges, which is what makes it robust
	// when edges are nearly parallel. In 2D the x and y components of that sum
	// vanish identically and only the z term survives, which is twice the
	// shoelace area - so the robust formulation and the schoolbook one turn
	// out to be the same sum, and it costs one multiply-subtract per edge.
	//
	// Vertices are taken relative to the first before anything is multiplied.
	// Without that the products are the size of the world squared - near 3e7
	// out where the levels run, where consecutive floats are 2 apart - and the
	// sum must then cancel back down to the size of the polygon. The error
	// that survives is set by the coordinates, not by the shape, so it is a
	// rounding artefact on a large polygon and comparable to the answer on a
	// small one: a 13.3-unit square at (6232.75, 5408.46) comes out as 176.0
	// against a true 176.88. Working relative to a vertex makes every product
	// the size of the polygon instead.
	float signed_area(std::span<const mattmath::Point2F> polygon);

	// Whether a point lies inside a convex polygon, treating the boundary as
	// inside.
	//
	// The test is one signed area per edge: a point is inside a convex shape
	// exactly when it is on the same side of every edge (9.1, step 1). No
	// division, no allocation, no triangulation, and it terminates early on
	// the first edge that disagrees.
	//
	// Winding-agnostic by construction - it asks whether the signs agree, not
	// whether they are positive - because nothing in this library constrains
	// the order a caller lists vertices in. A point exactly on an edge or a
	// vertex produces a zero, which agrees with either sign and so counts as
	// inside.
	//
	// Convexity is a precondition and is NOT checked; a concave polygon will
	// report false for points that are genuinely inside it.
	//
	// Anything that encloses nothing contains nothing, which is the same
	// answer signed_area gives for the same inputs: fewer than three
	// vertices, coincident vertices, all vertices collinear. A degenerate
	// polygon is therefore always false - including for points lying on it,
	// which is the one place this deliberately differs from "the boundary is
	// inside". A shape with no interior has no boundary to be on.
	//
	// A NaN in the point or in any vertex is false, by the rule test_AABB_AABB
	// states: the accept branch is one a comparison has to reach.
	bool point_in_convex_polygon(std::span<const mattmath::Point2F> polygon,
		const mattmath::Point2F& p);

	// Whether segments ab and cd cross PROPERLY, and if so where: t along ab,
	// and the point itself.
	//
	// Proper is the whole contract, and it is exclusive at both ends
	// (5.1.9.1, pp.152-153). Two segments that merely touch - one's endpoint
	// landing on the other, or on the other's endpoint - do not cross, and
	// neither do two collinear segments that overlap along their length, however
	// much of it they share. Both cases produce a zero signed area, and zero is
	// not strictly opposite anything.
	//
	// That agrees with manifold.h, where shapes which merely touch do not
	// overlap, and it is why the polygon routines that call this run a closed
	// containment pass over the vertices first: containment is what catches a
	// flush contact, and this is what catches a crossing. Neither is asked to
	// do the other's job.
	//
	// `t` and `p` are written only when the result is true.
	bool test_2D_segment_segment(const mattmath::Point2F& a,
		const mattmath::Point2F& b, const mattmath::Point2F& c,
		const mattmath::Point2F& d, float& t, mattmath::Point2F& p);

	// Whether p lies in triangle abc, boundary included, for either winding.
	//
	// This is point_in_convex_polygon over three vertices, and it is written
	// that way rather than duplicated so the engine has one definition of
	// "inside" (5.4.2, pp.203-206). The barycentric form it replaces divided
	// by the triangle's determinant, which is zero for a collinear triangle -
	// and every comparison against the resulting NaN was false, so a
	// degenerate triangle reported "outside" for every point in the plane
	// rather than reporting anything a caller could act on.
	//
	// A triangle with no area answers false for every point, which is
	// deliberate and is not the old NaN behaviour wearing a new hat: it is
	// what signed_area says about the same three points, reached by a
	// comparison rather than by a division nobody guarded. The sign form
	// first shipped returning TRUE for points on a collinear triangle's
	// supporting line - the whole infinite line, not merely the part between
	// the vertices - and true for every point in the plane once two vertices
	// coincided. Degenerate in, "contains nothing" out.
	bool test_point_triangle(const mattmath::Point2F& p,
		const mattmath::Point2F& a, const mattmath::Point2F& b,
		const mattmath::Point2F& c);

	// The point d on segment ab closest to c, and the t with d = a + t(b - a).
	//
	// A degenerate segment - a and b the same point - returns that point with
	// t = 0 rather than dividing by its zero length. The division is deferred
	// until both clamps have been decided, so it runs only where the divisor
	// is known positive (5.1.2, p.129), which also makes the two clamped
	// cases cheaper than the form on the previous page.
	void closest_pt_point_segment(const mattmath::Point2F& c, const mattmath::Point2F& a,
		const mattmath::Point2F& b, float& t, mattmath::Point2F& d);

	// Given point p, return point q on (or in) OBB b, closest to p
	void closest_pt_point_OBB(const mattmath::Point2F& p, const mattmath::OBB& b,
		mattmath::Point2F& q);

}

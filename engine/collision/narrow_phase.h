#pragma once

#include "engine/collision/manifold.h"
#include "engine/math/shape.h"

#include <optional>

namespace labrador
{
	// Measures one pair of shapes: nullopt when they do not overlap, and
	// otherwise the axis of least penetration and how deep it is, with the
	// normal pointing from `a` towards `b`.
	//
	// This is the separating-axis theorem, run over both shapes' edge normals,
	// and the axis it keeps is the one with the smallest overlap. That single
	// choice is the fix for the resolver this replaces, whose four defects
	// were all the same defect wearing different clothes - it classified the
	// contact by asking which of the collider's bounding-box edges the other
	// shape crossed, and that question has no answer for the cases that
	// matter:
	//
	//   - A wide thin platform crosses a standing player's left AND right
	//     edges. Comparing centres horizontally there shoves the player
	//     sideways off a platform they should have landed on. The
	//     minimum-overlap axis for that pair is vertical, always, because the
	//     platform is thin - which is the same fact the player can see.
	//   - One shape wholly containing the other crosses all four edges or
	//     none, so an edge-crossing classification has nothing to say about it.
	//     Containment has a well-defined minimum translation and this returns
	//     it.
	//   - A diagonal classification renormalised by std::max over the two
	//     *signed* components divides a left-and-up contact by the negative
	//     one and resolves backwards. There are no diagonal classifications
	//     here; there is one axis, and it came from an edge.
	//   - Bisection - 40 iterations of moving the shape back and forth by a
	//     shrinking fraction of its own size - has an unreportable failure and
	//     an approximate answer when it converges. This is analytic
	//     (PHILOSOPHY, Collision) and runs in the number of edges.
	//
	// The axis set is COMPLETE, not a good-enough sample of one. In two
	// dimensions, testing the face normals of both shapes decides convex
	// overlap exactly - Ericson says so outright (4.4.1, p.102): the test
	// "is either body wholly outside a face of the other" works in 2D and
	// fails only in 3D, and the nine cross-product axes that published 3D SAT
	// code carries exist solely to catch an edge-against-edge configuration
	// that has no counterpart when nothing can be skew. There is no missing
	// axis here to be found later, and adding cross-product axes imported from
	// a 3D implementation would be adding noise, not rigour. This is also the
	// answer to "should this be GJK": not for correctness, because this is
	// already exact.
	//
	// Two further reductions, both provably free. Opposite edges of a
	// rectangle have antiparallel normals, and an axis and its negative give
	// the same penetration - projecting on -n negates and swaps the interval,
	// which swaps the two ways out, so their minimum is unchanged and the
	// normal still comes back correctly signed. A rectangle therefore
	// contributes two axes, not four, and for an axis-aligned one they are the
	// cardinal directions and cost nothing to produce.
	//
	// CONVEXITY IS A PRECONDITION, and it is the one thing here that is not
	// self-enforcing. The separating-axis theorem decides nothing for a
	// concave shape: it will report a confident overlap and a meaningless
	// normal. Quad's constructor checks it. Triangle's does not - a collinear
	// triangle is constructible today - and RectangleF and RectangleRotated
	// are convex by construction. Degenerate edges are skipped rather than
	// contributing a bogus axis, and a shape left with no axes at all reports
	// no contact.
	//
	// Shapes that merely touch do not overlap: a zero-depth contact has no
	// meaningful normal, so it is not a contact. This is deliberately the
	// opposite convention to the bounding-box filter that runs before it,
	// which is closed and counts a shared edge as intersecting. A closed
	// filter feeding an open decider is the correct pairing: the cheap test
	// must never reject something the expensive one would have accepted, and
	// the expensive one owns the answer. See contacts.h.
	//
	// A coordinate that is not a number reports no contact. Every comparison
	// against NaN is false, so the axis test is written to make overlap
	// something a comparison has to reach rather than something reached by
	// falling through - a missed collision is survivable, and a manifold
	// carrying a NaN penetration is not, because it goes on to be applied to
	// a position.
	//
	// Supported shape types are the convex polygons - rectangle, rotated
	// rectangle, triangle and quad. A circle throws std::invalid_argument
	// rather than reporting no overlap, because silently missing collisions is
	// the worse failure and nothing in the tree is a collidable circle. Adding
	// them is one more axis, the centre-to-closest-point one.
	std::optional<Manifold> narrow_phase(const mattmath::Shape& a,
		const mattmath::Shape& b);
}

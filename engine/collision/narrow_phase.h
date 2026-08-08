#pragma once

#include "engine/collision/manifold.h"
#include "engine/math/matt_math.h"

#include <optional>

namespace artattack
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
	//     edges, so the old code compared centres horizontally and shoved the
	//     player sideways off a platform they should have landed on. The
	//     minimum-overlap axis for that pair is vertical, always, because the
	//     platform is thin - which is the same fact the player can see.
	//   - One shape wholly containing the other crossed all four edges or
	//     none, and fell through to a helper whose comparison contradicted its
	//     own comment. Containment has a well-defined minimum translation and
	//     this returns it.
	//   - A diagonal classification was renormalised by std::max over the two
	//     *signed* components, so a left-and-up contact divided by the
	//     negative one and resolved backwards. There are no diagonal
	//     classifications here; there is one axis, and it came from an edge.
	//   - Anything that was not two rectangles was resolved by bisection - 40
	//     iterations of moving the shape back and forth by a shrinking
	//     fraction of its own size - whose failure was unreportable and whose
	//     answer, when it did converge, was approximate. This is analytic
	//     (PHILOSOPHY, Collision) and runs in the number of edges.
	//
	// Shapes that merely touch do not overlap: a zero-depth contact has no
	// meaningful normal, so it is not a contact.
	//
	// Supported shape types are the convex polygons - rectangle, rotated
	// rectangle, triangle and quad. A circle throws std::invalid_argument
	// rather than reporting no overlap, because silently missing collisions is
	// the worse failure and nothing in the tree is a collidable circle. Adding
	// them is one more axis, the centre-to-closest-point one.
	std::optional<Manifold> narrow_phase(const mattmath::Shape& a,
		const mattmath::Shape& b);
}

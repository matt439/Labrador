#pragma once

#include "engine/math/vector2f.h"

namespace labrador
{
	// The smallest translation that moves an object out of a contact.
	//
	// `normal` and `penetration` are the pair CollisionObject::on_contact
	// receives, so the normal points from the object being moved towards the
	// thing it hit and this always moves it away.
	//
	// The engine offers this rather than performing it, because who moves is a
	// gameplay question the engine cannot answer: a player separates from a
	// wall, a projectile does not separate from anything - it dies - and the
	// wall never moves at all.
	mattmath::Vector2F separation(const mattmath::Vector2F& normal,
		float penetration);

	// How oblique an axis may be to the normal before separating along it is
	// treated as a broken request rather than an expensive one.
	//
	// Both vectors are unit, so dot(normal, axis) is the cosine of the angle
	// between them and the travel needed is penetration / that cosine. This
	// bound is therefore also a bound on the answer: the translation is never
	// more than ten times the overlap it closes. Past roughly 84 degrees off
	// the normal the axis stops being a slightly awkward direction and starts
	// being the wrong one - separating a wall contact by moving straight down
	// is not a costly manoeuvre, it is a category error, and the caller wants
	// separation() instead.
	constexpr float MIN_AXIS_ALIGNMENT = 0.1f;

	// The smallest translation *along `axis`* that does the same.
	//
	// A platformer wants this more often than it wants separation(): resolving
	// a slope contact along its own normal slides the player back down the
	// hill, so ground contacts separate straight up instead. The distance is
	// exact rather than approximate - a translation of length L along `axis`
	// closes L * dot(normal, axis) of the overlap, so the length that closes
	// all of it is penetration / dot(normal, axis).
	//
	// Preconditions, checked, throwing std::invalid_argument and naming the
	// offending value:
	//
	//   - `normal` and `axis` are unit length. The arithmetic is a cosine only
	//     if they are, and every quantity here is measured in the units the
	//     caller's world uses.
	//   - `penetration` is greater than zero, which is what a manifold
	//     promises - a contact that does not overlap is not a contact.
	//   - `axis` is no more than MIN_AXIS_ALIGNMENT away from being useless.
	//
	// It throws rather than returning zero, or an optional, or a very large
	// vector. The version this replaces guarded the divisor and returned zero
	// only when it was smaller than mattmath::EPSILON - so a dot product of
	// 1.0001e-4 passed the guard and returned a translation ten thousand times
	// the penetration, which the caller then applied to a position. A silent
	// teleport out of the one primitive whose job is safe arithmetic is
	// exactly the failure T6 exists to prevent, and there is no answer to
	// return that the caller could have used: an axis perpendicular to the
	// normal does not separate the pair however far you travel along it, so
	// zero was never a translation - it was a shrug.
	mattmath::Vector2F separation_along(const mattmath::Vector2F& normal,
		float penetration, const mattmath::Vector2F& axis);

	// `velocity` with the component that drives it into the surface removed,
	// and the component along the surface left alone. `normal` must be unit
	// length, and points the same way as it does everywhere else here: from
	// the object being moved towards the thing it hit.
	//
	// Any vector splits uniquely into a part along a direction and a part
	// perpendicular to it (Ericson, Real-Time Collision Detection, 3.3.3).
	// For a unit normal the first part is normal * dot(velocity, normal), so
	// the second - the part a slope should leave untouched - is one
	// subtraction. In 2D there is only one perpendicular direction, so the
	// answer is unambiguous without a basis, a trig call or a square root.
	//
	// A velocity already moving away from the surface is returned unchanged.
	// dot(velocity, normal) <= 0 means the contact is not what is stopping
	// this object, and a resolver that clamps anyway deletes a jump on the
	// frame it starts.
	//
	// This is what a slope contact wants instead of zeroing an axis. Zeroing
	// the vertical component of a velocity on a ramp deletes the whole climb
	// rate, so running up a slope is slower than running along the flat and
	// landing on one stops dead rather than sliding; the tangential speed the
	// player expects to keep is exactly the part this keeps. As with
	// separation(), the engine offers the arithmetic and never performs it -
	// whether a contact should cost an object its speed is a gameplay
	// question.
	mattmath::Vector2F slide(const mattmath::Vector2F& velocity,
		const mattmath::Vector2F& normal);
}

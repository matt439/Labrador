#pragma once

#include "engine/math/matt_math.h"

namespace artattack
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

	// The smallest translation *along `axis`* that does the same. `axis` must
	// be unit length.
	//
	// A platformer wants this more often than it wants separation(): resolving
	// a slope contact along its own normal slides the player back down the
	// hill, so ground contacts separate straight up instead. The distance is
	// exact rather than approximate - a translation of length L along `axis`
	// closes L * dot(normal, axis) of the overlap, so the length that closes
	// all of it is penetration / dot(normal, axis).
	//
	// Returns zero when `axis` is perpendicular to `normal`, because then no
	// translation along it separates the pair however far it goes.
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

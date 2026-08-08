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
}

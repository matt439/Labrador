#pragma once

#include "engine/collision/collision_layer.h"
#include "engine/core/game_object.h"
#include "engine/math/matt_math.h"

namespace artattack
{
	// A GameObject the collision module can see.
	//
	// This is finding #13's home. The interface it replaces lived in
	// game/objects/, took a game enum in its own signature and asked each
	// object two questions the engine should never have delegated:
	// `is_colliding(other)` - "are you two overlapping?" - and, by way of the
	// same predicate, "should you be tested at all?". Six classes answered the
	// first with the same copied AABB-then-narrow block, and the second with
	// hand-written type lists that had already diverged (see
	// collision_layer.h). Both questions are the engine's, and both are
	// answered once, in find_contacts.
	//
	// What is left is what only the object can know: its shape, which groups
	// it belongs to and responds to, what it is in the game's own vocabulary,
	// and what a contact means to it.
	class CollisionObject : public GameObject
	{
	public:
		~CollisionObject() override = default;

		// The fine half of the pair GameObject::bounds() opens. bounds() is
		// the coarse extent a broad phase indexes; this is the geometry the
		// narrow phase measures. Never null.
		virtual const mattmath::Shape* shape() const = 0;

		virtual CollisionLayer layer() const = 0;
		virtual CollisionMask mask() const = 0;
		virtual CollisionTag tag() const = 0;

		// Something overlapped this object, this frame.
		//
		// `normal` is a unit vector pointing from this object towards `other`,
		// and `penetration` is how far they overlap along it - so the two
		// participants of one contact are told the same overlap with opposite
		// normals, and each is told exactly once. That is the fix for a
		// dispatch that fired *both* objects' responses off *one* object's
		// predicate, and then tested every pair a second time with the roles
		// reversed.
		//
		// Separation is the game's call, not the engine's, which is why this
		// hands over the measurement rather than the movement: a one-way
		// platform separates only when the contact comes from above, a ramp
		// separates vertically rather than along the slope, and a projectile
		// does not separate at all - it dies. engine/collision/resolve.h has
		// the arithmetic for the cases that do separate.
		virtual void on_contact(const CollisionObject& other,
			const mattmath::Vector2F& normal, float penetration) = 0;

		// Retired at the end of this tick. Objects flagged mid-frame generate
		// no further contacts, so a projectile that hits a wall cannot also
		// hit the player standing behind it.
		virtual bool for_deletion() const = 0;

		// Requests removal at the end of the frame. Objects that are part of
		// the level's fixed geometry (structures, paint tiles) cannot be
		// removed and ignore this by default.
		virtual void set_for_deletion(bool /*for_deletion*/) {}
	};
}

#pragma once

#include "engine/collision/collision_layer.h"
#include "engine/core/game_object.h"
#include "engine/math/shape.h"
#include "engine/math/vector2f.h"

namespace labrador
{
	// A GameObject the collision module can see.
	//
	// An interface that asks each object `is_colliding(other)` - "are you two
	// overlapping?" - and, by way of the same predicate, "should you be
	// tested at all?" delegates two questions the engine should keep: every
	// class then answers the first with the same copied AABB-then-narrow block
	// and the second with a hand-written type list, and the lists diverge (see
	// collision_layer.h). Both are answered once, in find_contacts.
	//
	// What is left is what only the object can know: its shape, which groups
	// it belongs to and responds to, what it is in the game's own vocabulary,
	// and what a contact means to it.
	class CollisionObject : public GameObject
	{
	public:
		~CollisionObject() override = default;

		// One Shape per collision object. The broad phase and bounding-box
		// filter index its bounding_box(); collision never reads GameObject::bounds().
		// The returned pointer is borrowed, never null, and its geometry must
		// remain valid for the collision queries using it.
		virtual const Shape* shape() const = 0;

		// Throws std::invalid_argument when shape() is null, has no supported
		// type, or its defining coordinates/extents contain a NaN or infinity.
		// The message names this object's tag, shape kind and offending field.
		// Reads only; never changes or retires the object.
		// Finite degenerate/negative geometry keeps its predicate semantics;
		// overflow in a finite shape's derived bounding box is not rejected.
		void validate_finite_shape() const;

		virtual CollisionLayer layer() const = 0;
		virtual CollisionMask mask() const = 0;
		virtual CollisionTag tag() const = 0;

		// Something overlapped this object, this frame.
		//
		// `normal` is a unit vector pointing from this object towards `other`,
		// and `penetration` is how far they overlap along it - so the two
		// participants of one contact are told the same overlap with opposite
		// normals, and each is told exactly once.
		//
		// Separation is the game's call, not the engine's, which is why this
		// hands over the measurement rather than the movement: a one-way
		// platform separates only when the contact comes from above, a ramp
		// separates vertically rather than along the slope, and a projectile
		// does not separate at all - it dies. engine/collision/resolve.h has
		// the arithmetic for the cases that do separate.
		virtual void on_contact(const CollisionObject& other,
			const Vector2F& normal, float penetration) = 0;

		// Retired at the end of this tick. Objects flagged mid-frame generate
		// no further contacts, so a projectile that hits a wall cannot also
		// hit the player standing behind it.
		virtual bool for_deletion() const = 0;

		// Requests removal at the end of the tick, and is the only way the
		// scene's bounds sweep (Scene::set_bounds) retires an object that has
		// left the world.
		//
		// THE DEFAULT DOES NOTHING, AND THE SCENE RETIRES ONLY WHAT
		// for_deletion() ANSWERS TRUE FOR. A collidable that keeps this default
		// is never retired by the sweep or by any other caller of this - only
		// Scene::remove, the game naming the object itself, takes it out. It
		// goes on updating, drawing and colliding outside the bounds, the
		// sweep asks again every tick, and nothing reports it. That is the right answer
		// for a level's fixed geometry (walls, platforms), which must not be
		// lost that way. Anything that can leave the world - a projectile -
		// overrides this to keep the flag and returns it from for_deletion().
		virtual void set_for_deletion(bool /*for_deletion*/) {}
	};
}

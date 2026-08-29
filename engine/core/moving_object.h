#pragma once

#include "engine/math/vector2f.h"

namespace labrador
{
	// A velocity and a rotation, with the accessors that are called.
	//
	// Twenty bytes of state and no accessor families around them. Three of
	// those (unit_*, *_magnitude, *_angle, alter_*) come to 27 virtual
	// protected accessors with about one real override between them, on the
	// base class of every player and projectile a client has - which is a
	// vtable per object for calls nobody makes. A displacement written and
	// read inside a single call chain is not a member either: it is a return
	// value and a local.
	//
	// Not virtual. Nothing overrode any of these, and a virtual accessor on
	// the object the simulation touches most is a cost with no client
	// (PHILOSOPHY, T8).
	class MovingObject
	{
	public:
		virtual ~MovingObject() = default;
		MovingObject() = default;

		explicit MovingObject(const mattmath::Vector2F& velocity,
			float rotation = 0.0f);

	protected:
		const mattmath::Vector2F& velocity() const;
		float velocity_x() const;
		float velocity_y() const;

		void set_velocity(const mattmath::Vector2F& velocity);
		void set_velocity_x(float x);
		void set_velocity_y(float y);

		void alter_velocity_x(float x);
		void alter_velocity_y(float y);

		float rotation() const;
		void set_rotation(float rotation);

	private:
		mattmath::Vector2F velocity_ = mattmath::Vector2F::ZERO;
		float rotation_ = 0.0f;
	};
}

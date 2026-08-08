#pragma once

#include "engine/math/matt_math.h"

namespace artattack
{
	// A velocity and a rotation, with the accessors that are called.
	//
	// It used to carry 27 virtual protected accessors around twenty bytes of
	// state, with one real override between them - three whole families
	// (unit_*, *_magnitude, *_angle, alter_*) that nothing had ever called,
	// each one virtual, on the base class of every player and projectile in
	// the game. They are gone, and so is the third member: dx_ was a
	// displacement written and read inside a single call chain, so
	// Projectile::update_movement returns it now and Player keeps it in a
	// local.
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

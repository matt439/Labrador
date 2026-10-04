#pragma once

#include "engine/math/vector2f.h"

namespace labrador
{
	// A velocity and a rotation, for a derived class to read and write.
	//
	// Plain accessors, none of them virtual: a virtual accessor on the object
	// the simulation touches most is a cost with no client (T8). A
	// displacement written and read inside a single call chain is not a
	// member: it is a return value and a local.
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

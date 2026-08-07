#pragma once

#include "engine/math/matt_math.h"

namespace artattack
{
	class MovingObject
	{
	public:
		virtual ~MovingObject() = default;
		MovingObject() = default;

		explicit MovingObject(const mattmath::Vector2F& velocity,
			float rotation = 0.0f,
			const mattmath::Vector2F& dx = mattmath::Vector2F::ZERO);

	protected:
		virtual const mattmath::Vector2F& velocity() const;
		virtual float velocity_x() const;
		virtual float velocity_y() const;

		virtual void set_velocity(const mattmath::Vector2F& velocity);
		virtual void set_velocity_x(float x);
		virtual void set_velocity_y(float y);

		virtual void alter_velocity(const mattmath::Vector2F& velocity);
		virtual void alter_velocity_x(float x);
		virtual void alter_velocity_y(float y);

		virtual mattmath::Vector2F unit_velocity() const;
		virtual float velocity_magnitude() const;
		virtual float velocity_angle() const;

		virtual const mattmath::Vector2F& dx() const;
		virtual float dx_x() const;
		virtual float dx_y() const;

		virtual void set_dx(const mattmath::Vector2F& dx);
		virtual void set_dx_x(float x);
		virtual void set_dx_y(float y);

		virtual void alter_dx(const mattmath::Vector2F& dx);
		virtual void alter_dx_x(float x);
		virtual void alter_dx_y(float y);

		virtual mattmath::Vector2F unit_dx() const;
		virtual float dx_magnitude() const;
		virtual float dx_angle() const;

		virtual float rotation() const;
		virtual void set_rotation(float rotation);
		virtual void alter_rotation(float rotation);

	private:
		mattmath::Vector2F velocity_ = mattmath::Vector2F::ZERO;
		mattmath::Vector2F dx_ = mattmath::Vector2F::ZERO;
		float rotation_ = 0.0f;

	};
}

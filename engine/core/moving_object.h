#pragma once

#include "engine/math/matt_math.h"

class MovingObject
{
public:
	virtual ~MovingObject() = default;
	MovingObject() = default;

	explicit MovingObject(const MattMath::Vector2F& velocity,
		float rotation = 0.0f,
		const MattMath::Vector2F& dx = MattMath::Vector2F::ZERO);

protected:
	virtual const MattMath::Vector2F& get_velocity() const;
	virtual float get_velocity_x() const;
	virtual float get_velocity_y() const;

	virtual void set_velocity(const MattMath::Vector2F& velocity);
	virtual void set_velocity_x(float x);
	virtual void set_velocity_y(float y);

	virtual void alter_velocity(const MattMath::Vector2F& velocity);
	virtual void alter_velocity_x(float x);
	virtual void alter_velocity_y(float y);

	virtual MattMath::Vector2F get_unit_velocity() const;
	virtual float get_velocity_magnitude() const;
	virtual float get_velocity_angle() const;

	virtual const MattMath::Vector2F& get_dx() const;
	virtual float get_dx_x() const;
	virtual float get_dx_y() const;

	virtual void set_dx(const MattMath::Vector2F& dx);
	virtual void set_dx_x(float x);
	virtual void set_dx_y(float y);

	virtual void alter_dx(const MattMath::Vector2F& dx);
	virtual void alter_dx_x(float x);
	virtual void alter_dx_y(float y);

	virtual MattMath::Vector2F get_unit_dx() const;
	virtual float get_dx_magnitude() const;
	virtual float get_dx_angle() const;

	virtual float get_rotation() const;
	virtual void set_rotation(float rotation);
	virtual void alter_rotation(float rotation);

private:
	MattMath::Vector2F velocity_ = MattMath::Vector2F::ZERO;
	MattMath::Vector2F dx_ = MattMath::Vector2F::ZERO;
	float rotation_ = 0.0f;

};

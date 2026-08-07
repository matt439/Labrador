#include "engine/core/moving_object.h"

using namespace MattMath;

MovingObject::MovingObject(const Vector2F& velocity,
	float rotation,
	const Vector2F& dx) :
	_velocity(velocity),
	_dx(dx),
	_rotation(rotation)
{

}

const Vector2F& MovingObject::get_velocity() const
{
	return this->_velocity;
}
float MovingObject::get_velocity_x() const
{
	return this->_velocity.x;
}
float MovingObject::get_velocity_y() const
{
	return this->_velocity.y;
}
void MovingObject::set_velocity(const Vector2F& velocity)
{
	this->_velocity = velocity;
}
void MovingObject::set_velocity_x(float x)
{
	this->_velocity.x = x;
}
void MovingObject::set_velocity_y(float y)
{
	this->_velocity.y = y;
}
void MovingObject::alter_velocity(const Vector2F& velocity)
{
	this->_velocity += velocity;
}
void MovingObject::alter_velocity_x(float x)
{
	this->_velocity.x += x;
}
void MovingObject::alter_velocity_y(float y)
{
	this->_velocity.y += y;
}
Vector2F MovingObject::get_unit_velocity() const
{
	return this->_velocity.normalized();
}
float MovingObject::get_velocity_magnitude() const
{
	return this->_velocity.length();
}
float MovingObject::get_velocity_angle() const
{
	return atan2(this->_velocity.y, this->_velocity.x);
}
const Vector2F& MovingObject::get_dx() const
{
	return this->_dx;
}
float MovingObject::get_dx_x() const
{
	return this->_dx.x;
}
float MovingObject::get_dx_y() const
{
	return this->_dx.y;
}
void MovingObject::set_dx(const Vector2F& dx)
{
	this->_dx = dx;
}
void MovingObject::set_dx_x(float x)
{
	this->_dx.x = x;
}
void MovingObject::set_dx_y(float y)
{
	this->_dx.y = y;
}
void MovingObject::alter_dx(const Vector2F& dx)
{
	this->_dx += dx;
}
void MovingObject::alter_dx_x(float x)
{
	this->_dx.x += x;
}
void MovingObject::alter_dx_y(float y)
{
	this->_dx.y += y;
}
Vector2F MovingObject::get_unit_dx() const
{
	return this->_dx.normalized();
}
float MovingObject::get_dx_magnitude() const
{
	return this->_dx.length();
}
float MovingObject::get_dx_angle() const
{
	return atan2(this->_dx.y, this->_dx.x);
}
float MovingObject::get_rotation() const
{
	return this->_rotation;
}
void MovingObject::set_rotation(float rotation)
{
	this->_rotation = rotation;
}
void MovingObject::alter_rotation(float rotation)
{
	this->_rotation += rotation;
}
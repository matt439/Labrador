#include "engine/core/moving_object.h"

using namespace MattMath;

MovingObject::MovingObject(const Vector2F& velocity,
	float rotation,
	const Vector2F& dx) :
	velocity_(velocity),
	dx_(dx),
	rotation_(rotation)
{

}

const Vector2F& MovingObject::get_velocity() const
{
	return this->velocity_;
}
float MovingObject::get_velocity_x() const
{
	return this->velocity_.x;
}
float MovingObject::get_velocity_y() const
{
	return this->velocity_.y;
}
void MovingObject::set_velocity(const Vector2F& velocity)
{
	this->velocity_ = velocity;
}
void MovingObject::set_velocity_x(float x)
{
	this->velocity_.x = x;
}
void MovingObject::set_velocity_y(float y)
{
	this->velocity_.y = y;
}
void MovingObject::alter_velocity(const Vector2F& velocity)
{
	this->velocity_ += velocity;
}
void MovingObject::alter_velocity_x(float x)
{
	this->velocity_.x += x;
}
void MovingObject::alter_velocity_y(float y)
{
	this->velocity_.y += y;
}
Vector2F MovingObject::get_unit_velocity() const
{
	return this->velocity_.normalized();
}
float MovingObject::get_velocity_magnitude() const
{
	return this->velocity_.length();
}
float MovingObject::get_velocity_angle() const
{
	return atan2(this->velocity_.y, this->velocity_.x);
}
const Vector2F& MovingObject::get_dx() const
{
	return this->dx_;
}
float MovingObject::get_dx_x() const
{
	return this->dx_.x;
}
float MovingObject::get_dx_y() const
{
	return this->dx_.y;
}
void MovingObject::set_dx(const Vector2F& dx)
{
	this->dx_ = dx;
}
void MovingObject::set_dx_x(float x)
{
	this->dx_.x = x;
}
void MovingObject::set_dx_y(float y)
{
	this->dx_.y = y;
}
void MovingObject::alter_dx(const Vector2F& dx)
{
	this->dx_ += dx;
}
void MovingObject::alter_dx_x(float x)
{
	this->dx_.x += x;
}
void MovingObject::alter_dx_y(float y)
{
	this->dx_.y += y;
}
Vector2F MovingObject::get_unit_dx() const
{
	return this->dx_.normalized();
}
float MovingObject::get_dx_magnitude() const
{
	return this->dx_.length();
}
float MovingObject::get_dx_angle() const
{
	return atan2(this->dx_.y, this->dx_.x);
}
float MovingObject::get_rotation() const
{
	return this->rotation_;
}
void MovingObject::set_rotation(float rotation)
{
	this->rotation_ = rotation;
}
void MovingObject::alter_rotation(float rotation)
{
	this->rotation_ += rotation;
}
#include "engine/render/draw_object.h"

using namespace DirectX;
using namespace MattMath;

DrawObject::DrawObject(ResourceManager* resource_manager,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	_resource_manager(resource_manager),
	_colour(color),
	_draw_rotation(rotation),
	_origin(origin),
	_effects(effects),
	_layer_depth(layer_depth)
{

}
ResourceManager* DrawObject::get_resource_manager() const
{
	return this->_resource_manager;
}
const Colour& DrawObject::get_colour() const
{
	return this->_colour;
}
float DrawObject::get_draw_rotation() const
{
	return this->_draw_rotation;
}
const Vector2F& DrawObject::get_origin() const
{
	return this->_origin;
}
SpriteEffects DrawObject::get_effects() const
{
	return this->_effects;
}
float DrawObject::get_layer_depth() const
{
	return this->_layer_depth;
}
void DrawObject::set_colour(const Colour& colour)
{
	this->_colour = colour;
}
void DrawObject::set_draw_rotation(float rotation)
{
	this->_draw_rotation = rotation;
}
void DrawObject::set_origin(const Vector2F& origin)
{
	this->_origin = origin;
}
void DrawObject::set_effects(SpriteEffects effects)
{
	this->_effects = effects;
}
void DrawObject::set_layer_depth(float layer_depth)
{
	this->_layer_depth = layer_depth;
}
void DrawObject::set_draw_rotation_by_rectangle_rotated(const RectangleRotated& /*rect*/)
{
	// TODO: Implement this function
}
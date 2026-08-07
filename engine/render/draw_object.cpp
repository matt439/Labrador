#include "engine/render/draw_object.h"

using namespace DirectX;
using namespace MattMath;

DrawObject::DrawObject(RenderResources* render_resources,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	render_resources_(render_resources),
	colour_(color),
	draw_rotation_(rotation),
	origin_(origin),
	effects_(effects),
	layer_depth_(layer_depth)
{

}
RenderResources* DrawObject::get_render_resources() const
{
	return this->render_resources_;
}
const Colour& DrawObject::get_colour() const
{
	return this->colour_;
}
float DrawObject::get_draw_rotation() const
{
	return this->draw_rotation_;
}
const Vector2F& DrawObject::get_origin() const
{
	return this->origin_;
}
SpriteEffects DrawObject::get_effects() const
{
	return this->effects_;
}
float DrawObject::get_layer_depth() const
{
	return this->layer_depth_;
}
void DrawObject::set_colour(const Colour& colour)
{
	this->colour_ = colour;
}
void DrawObject::set_draw_rotation(float rotation)
{
	this->draw_rotation_ = rotation;
}
void DrawObject::set_origin(const Vector2F& origin)
{
	this->origin_ = origin;
}
void DrawObject::set_effects(SpriteEffects effects)
{
	this->effects_ = effects;
}
void DrawObject::set_layer_depth(float layer_depth)
{
	this->layer_depth_ = layer_depth;
}
void DrawObject::set_draw_rotation_by_rectangle_rotated(const RectangleRotated& /*rect*/)
{
	// TODO: Implement this function
}
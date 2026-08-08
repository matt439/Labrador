#include "engine/render/draw_object.h"

using namespace mattmath;

namespace artattack
{
	DrawObject::DrawObject(RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		render_resources_(render_resources),
		colour_(color),
		draw_rotation_(rotation),
		origin_(origin),
		flip_(flip),
		layer_depth_(layer_depth)
	{

	}
	RenderResources* DrawObject::render_resources() const
	{
		return this->render_resources_;
	}
	const Colour& DrawObject::colour() const
	{
		return this->colour_;
	}
	float DrawObject::draw_rotation() const
	{
		return this->draw_rotation_;
	}
	const Vector2F& DrawObject::origin() const
	{
		return this->origin_;
	}
	SpriteFlip DrawObject::flip() const
	{
		return this->flip_;
	}
	float DrawObject::layer_depth() const
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
	void DrawObject::set_flip(SpriteFlip flip)
	{
		this->flip_ = flip;
	}
	void DrawObject::set_layer_depth(float layer_depth)
	{
		this->layer_depth_ = layer_depth;
	}
	void DrawObject::set_draw_rotation_by_rectangle_rotated(const RectangleRotated& /*rect*/)
	{
		// TODO: Implement this function
	}
}

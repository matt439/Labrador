#include "engine/render/draw_object.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/vector2f.h"

#include <cmath>

using namespace mattmath;

namespace labrador
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
	// THE ANGLE THE RECTANGLE IS ALREADY TURNED BY, AND NOTHING ELSE. A
	// RectangleRotated carries its rotation as an orthonormal axis pair rather
	// than as a number (engine/math/rectangle_rotated.h says why: writing one
	// axis against the other is how a rotation gets rejected), so the angle is
	// atan2 of the x axis - clockwise-positive on screen, which is the seam's
	// convention because y runs down (renderer.h, and the pixel contract's
	// "a positive rotation turns the sprite clockwise on screen").
	//
	// IT SETS THE ROTATION AND NOT THE ORIGIN, which is the whole of the
	// decision here. A rectangle turns about its centre and a sprite turns
	// about its origin, so a caller that wants those to be the same point says
	// so with set_origin - in unscaled source texels, which this class cannot
	// convert to because it does not know the source. Setting both would make
	// this the one setter with two effects and would be wrong for every caller
	// whose origin is deliberate.
	//
	// It was an empty body with a TODO in it: a setter that took an argument
	// and discarded it, which is the one shape of wrong a caller cannot see.
	void DrawObject::set_draw_rotation_by_rectangle_rotated(
		const RectangleRotated& rect)
	{
		const Point2F axis = rect.x_axis();
		this->set_draw_rotation(std::atan2(axis.y, axis.x));
	}
}

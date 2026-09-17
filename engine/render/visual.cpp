#include "engine/render/visual.h"

#include "engine/render/sprite_geometry.h"

#include <string>

using namespace mattmath;

namespace labrador
{
	Visual::Visual(const std::string& sheet_name,
		const std::string& frame_name,
		const RectangleF& rectangle,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		TextureObject(sheet_name, frame_name, render_resources,
			color, rotation, origin, flip, layer_depth),
		rectangle_(rectangle)
	{
		this->bounds_ = this->drawn_bounds();
	}

	Visual::Visual(const std::string& sheet_name,
		const std::string& frame_name,
		const RectangleRotated& rect_rotated,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		TextureObject(sheet_name, frame_name, render_resources,
			color, rotation, origin, flip, layer_depth)
	{
		this->rectangle_ = rect_rotated.rectangle_rotated_to_axis();
		this->bounds_ = this->drawn_bounds();
	}


	void Visual::update(float /*dt*/)
	{
		// do nothing
	}
	void Visual::draw(DrawList& draw_list) const
	{
		this->TextureObject::draw(draw_list, this->rectangle_);
	}
	RectangleF Visual::bounds() const
	{
		return this->bounds_;
	}

	RectangleF Visual::drawn_bounds() const
	{
		// The frame's origin and the caller's, summed the way
		// SpriteSheet::draw sums them on the way to build_sprite_quad, so the
		// box answers for the quad that is actually built.
		const SpriteFrame& frame =
			this->sprite_sheet()->sprite_frame(this->frame());
		return sprite_quad_bounds(this->rectangle_, frame.source_rectangle(),
			this->draw_rotation(), frame.origin() + this->origin());
	}
}
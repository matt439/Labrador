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
		this->initialize_bounds();
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
		// The shape is centered; sprite destinations rotate about their top
		// left. Put that pivot at the center and add half the source to the
		// origin. Authored/caller origins remain extra local offsets.
		this->rectangle_.set_position(rect_rotated.center());
		const SpriteFrame& frame = this->sprite_sheet()->sprite_frame(this->frame());
		this->set_origin(origin + Vector2F(
			static_cast<float>(frame.source_rectangle().width) * 0.5f,
			static_cast<float>(frame.source_rectangle().height) * 0.5f));
		this->set_draw_rotation(rect_rotated.angle() + rotation);
		this->initialize_bounds();
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

	void Visual::initialize_bounds()
	{
		const SpriteFrame& frame = this->sprite_sheet()->sprite_frame(this->frame());
		const Vector2F origin = frame.origin() + this->origin();
		this->bounds_ = sprite_world_bounds(this->rectangle_, frame.source_rectangle(),
			this->draw_rotation(), origin);
		const RectangleF error = sprite_world_bounds(RectangleF::ZERO,
			frame.source_rectangle(), this->draw_rotation(), origin, 1.0f);
		this->pixel_padding_ = Vector2F(-error.x, -error.y);
	}

	RectangleF Visual::cull_bounds(float units_per_pixel) const
	{
		const Vector2F padding = this->pixel_padding_ * units_per_pixel;
		return RectangleF(this->bounds_.x - padding.x, this->bounds_.y - padding.y,
			this->bounds_.width + padding.x * 2.0f,
			this->bounds_.height + padding.y * 2.0f);
	}
}

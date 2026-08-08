#include "engine/render/visual.h"

using namespace mattmath;

namespace artattack
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
		return this->rectangle_;
	}
}
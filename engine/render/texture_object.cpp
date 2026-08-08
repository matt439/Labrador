#include "engine/render/texture_object.h"

using namespace mattmath;

namespace artattack
{
	TextureObject::TextureObject(const std::string& sheet_name,
		const std::string& frame_name,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		SpriteSheetObject(sheet_name, render_resources,
			color, rotation, origin, flip, layer_depth),
		// The base is complete by now, so its sheet is there to resolve against.
		frame_(SpriteSheetObject::sprite_sheet()->
			resolve_sprite_frame(frame_name))
	{

	}

	SpriteSheet::frame_handle TextureObject::frame() const
	{
		return this->frame_;
	}

	void TextureObject::set_frame(const std::string& sheet_name,
		const std::string& frame_name)
	{
		this->set_sprite_sheet(sheet_name);
		this->frame_ = this->sprite_sheet()->resolve_sprite_frame(frame_name);
	}

	void TextureObject::set_frame(const std::string& frame_name)
	{
		this->frame_ = this->sprite_sheet()->resolve_sprite_frame(frame_name);
	}

	void TextureObject::draw(DrawList& draw_list,
		const RectangleF& destination_rectangle) const
	{
		SpriteSheetObject::sprite_sheet()->draw(draw_list,
			this->frame_,
			destination_rectangle,
			this->colour(),
			this->draw_rotation(),
			this->origin(),
			this->flip(),
			this->layer_depth());
	}

	void TextureObject::draw(DrawList& draw_list,
		const Vector2F& position, float scale) const
	{
		SpriteSheetObject::sprite_sheet()->draw(draw_list,
			this->frame_,
			position,
			this->colour(),
			this->draw_rotation(),
			this->origin(),
			scale,
			this->flip(),
			this->layer_depth());
	}

	void TextureObject::draw_with(DrawList& draw_list,
		const RectangleF& destination_rectangle,
		SpriteSheet::frame_handle frame,
		const Colour& colour,
		const Vector2F& origin,
		SpriteFlip flip,
		float rotation) const
	{
		SpriteSheetObject::sprite_sheet()->draw(draw_list,
			frame,
			destination_rectangle,
			colour,
			rotation,
			origin,
			flip,
			this->layer_depth());
	}
}

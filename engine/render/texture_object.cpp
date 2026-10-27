#include "engine/render/texture_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <stdexcept>
#include <string>
#include <tuple>

using namespace labrador;

namespace labrador
{
	namespace
	{
		const RenderResources::SheetFrame& resolved(
			const RenderResources::SheetFrame& frame)
		{
			if (!frame.sheet().valid() || !frame.frame().valid())
			{
				throw std::out_of_range("TextureObject was given an "
					"unresolved SheetFrame; RenderResources::"
					"resolve_sheet_frame makes a resolved one.");
			}
			return frame;
		}
	}

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

	TextureObject::TextureObject(RenderResources::SheetFrame frame,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) :
		SpriteSheetObject(resolved(frame).sheet(), render_resources,
			color, rotation, origin, flip, layer_depth),
		frame_(frame.frame())
	{
		// Both halves are read through the table here, once, so a pair that
		// names a slot it does not have throws now rather than at a draw.
		std::ignore = this->sprite_sheet()->sprite_frame(this->frame_);
	}

	SpriteSheet::frame_handle TextureObject::frame() const
	{
		return this->frame_;
	}

	void TextureObject::set_frame(RenderResources::SheetFrame frame)
	{
		// Checked against the table before anything is assigned, so a throw
		// leaves the object drawing what it drew.
		std::ignore = this->render_resources()->sprite_sheet(
			resolved(frame).sheet())->sprite_frame(frame.frame());

		this->set_sprite_sheet(frame.sheet());
		this->frame_ = frame.frame();
	}

	void TextureObject::set_frame(const std::string& sheet_name,
		const std::string& frame_name)
	{
		this->set_frame(this->render_resources()->resolve_sheet_frame(
			sheet_name, frame_name));
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

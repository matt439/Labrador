#include "engine/render/texture_object.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
	TextureObject::TextureObject(const std::string& sheet_name,
		const std::string& frame_name,
		RenderResources* render_resources,
		const Colour& color,
		float rotation,
		const Vector2F& origin,
		SpriteEffects effects,
		float layer_depth) :
		SpriteSheetObject(sheet_name, render_resources,
			color, rotation, origin, effects, layer_depth),
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

	void TextureObject::draw(SpriteBatch* sprite_batch,
		const RectangleI& destination_rectangle) const
	{
		SpriteSheet* sprite_sheet = SpriteSheetObject::sprite_sheet();

		sprite_sheet->draw(sprite_batch,
							this->frame_,
							destination_rectangle,
							this->colour(),
							this->draw_rotation(),
							this->origin(),
							this->effects(),
							this->layer_depth());
	}
	void TextureObject::draw(SpriteBatch* sprite_batch,
		const RectangleF& destination_rectangle)const
	{
		this->draw(sprite_batch, destination_rectangle.rectangle_i());
	}
	void TextureObject::draw(SpriteBatch* sprite_batch,
		const Vector2F& position, float scale) const
	{
		SpriteSheet* sprite_sheet = SpriteSheetObject::sprite_sheet();

		sprite_sheet->draw(sprite_batch,
							this->frame_,
							position,
							this->colour(),
							this->draw_rotation(),
							this->origin(),
							scale,
							this->effects(),
							this->layer_depth());
	}
	void TextureObject::draw(SpriteBatch* sprite_batch,
		const RectangleF& destination_rectangle,
		const Camera& camera) const
	{
		RectangleF rect = camera.calculate_view_rectangle(destination_rectangle);
		this->draw(sprite_batch, rect);
	}
	void TextureObject::draw(SpriteBatch* sprite_batch,
		const Vector2F& position,
		const Camera& camera, float scale) const
	{
		Vector2F view_pos = camera.calculate_view_position(position);
		float view_scale = camera.calculate_view_scale(scale);
		this->draw(sprite_batch, view_pos, view_scale);
	}

	void TextureObject::draw_with(SpriteBatch* sprite_batch,
		const RectangleF& destination_rectangle,
		const Camera& camera,
		SpriteSheet::frame_handle frame,
		const Colour& colour,
		const Vector2F& origin,
		SpriteEffects effects,
		float rotation) const
	{
		SpriteSheet* sprite_sheet = SpriteSheetObject::sprite_sheet();

		sprite_sheet->draw(sprite_batch,
			frame,
			camera.calculate_view_rectangle(destination_rectangle).rectangle_i(),
			colour,
			rotation,
			origin,
			effects,
			this->layer_depth());
	}
}

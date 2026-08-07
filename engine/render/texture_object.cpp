#include "engine/render/texture_object.h"

using namespace DirectX;
using namespace MattMath;

TextureObject::TextureObject(const std::string& sheet_name,
	const std::string& frame_name,
	ResourceManager* resource_manager,
	const Colour& color,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	SpriteSheetObject(sheet_name, frame_name, resource_manager,
		color, rotation, origin, effects, layer_depth)
{

}

void TextureObject::draw(SpriteBatch* sprite_batch,
	const RectangleI& destination_rectangle) const
{
	SpriteSheet* sprite_sheet = SpriteSheetObject::get_sprite_sheet();

	sprite_sheet->draw(sprite_batch,
						this->get_element_name(),
						destination_rectangle,
						this->get_colour(),
						this->get_draw_rotation(),
						this->get_origin(),
						this->get_effects(),
						this->get_layer_depth());
}
void TextureObject::draw(SpriteBatch* sprite_batch,
	const RectangleF& destination_rectangle)const
{
	this->draw(sprite_batch, destination_rectangle.get_rectangle_i());
}
void TextureObject::draw(SpriteBatch* sprite_batch,
	const Vector2F& position, float scale) const
{
	SpriteSheet* sprite_sheet = SpriteSheetObject::get_sprite_sheet();
	
	sprite_sheet->draw(sprite_batch,
						this->get_element_name(),
						position,
						this->get_colour(),
						this->get_draw_rotation(),
						this->get_origin(),
						scale,
						this->get_effects(),
						this->get_layer_depth());
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
	const std::string& element_name,
	const Colour& colour,
	const Vector2F& origin,
	SpriteEffects effects,
	float rotation) const
{
	SpriteSheet* sprite_sheet = SpriteSheetObject::get_sprite_sheet();

	sprite_sheet->draw(sprite_batch,
		element_name,
		camera.calculate_view_rectangle(destination_rectangle).get_rectangle_i(),
		colour,
		rotation,
		origin,
		effects,
		this->get_layer_depth());
}
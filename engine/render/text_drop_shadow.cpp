#include "engine/render/text_drop_shadow.h"

using namespace MattMath;
using namespace DirectX;

TextDropShadow::TextDropShadow(const std::string& text,
	const std::string& font_name,
	const Vector2F& position,
	ResourceManager* resource_manager,
	const Colour& color,
	const Colour& shadow_color,
	const Vector2F& shadow_offset,
	float scale,
	float shadow_scale,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	Text(text, font_name, position, resource_manager,
	     color, scale, rotation, origin, effects, layer_depth),
	_shadow_offset(shadow_offset),
	_shadow_color(shadow_color),
	_shadow_scale(shadow_scale)
{

}
void TextDropShadow::draw(SpriteBatch* sprite_batch, const Camera& camera)
{
	// Pass the shadow's colour, offset position and scale rather than assigning
	// them to this object and restoring them afterwards. The save/restore was a
	// data race: the render workers all run draw() on the same object, so one
	// thread could restore the originals while another was mid-shadow.
	this->TextObject::draw_with(sprite_batch, camera,
		this->_shadow_color,
		this->get_position() + this->_shadow_offset,
		this->_shadow_scale);

	this->TextObject::draw(sprite_batch, camera);
}
void TextDropShadow::draw(SpriteBatch* sprite_batch)
{
	this->draw(sprite_batch, Camera::DEFAULT_CAMERA);
}
Vector2F TextDropShadow::get_shadow_offset() const
{
	return this->_shadow_offset;
}
Colour TextDropShadow::get_shadow_color() const
{
	return this->_shadow_color;
}
float TextDropShadow::get_shadow_scale() const
{
	return this->_shadow_scale;
}
void TextDropShadow::set_shadow_offset(const Vector2F& offset)
{
	this->_shadow_offset = offset;
}
void TextDropShadow::set_shadow_color(const Colour& color)
{
	this->_shadow_color = color;
}
void TextDropShadow::set_shadow_scale(float scale)
{
	this->_shadow_scale = scale;
}
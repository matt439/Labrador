#include "engine/render/text_object.h"

using namespace DirectX;
using namespace mattmath;

TextObject::TextObject(const std::string& text,
	const std::string& font_name,
	const Vector2F& position,
	RenderResources* render_resources,
	const Colour& color,
	float scale,
	float rotation,
	const Vector2F& origin,
	SpriteEffects effects,
	float layer_depth) :
	DrawObject(render_resources, color,
		rotation, origin, effects, layer_depth),
	text_(text),
	font_(render_resources->resolve_sprite_font(font_name)),
	position_(position),
	scale_(scale)
{

}

void TextObject::draw(SpriteBatch* sprite_batch, const Camera& camera) const
{
	const SpriteFont* sprite_font =
		this->get_render_resources()->get_sprite_font(this->font_);

	Vector2F view_pos = camera.calculate_view_position(this->position_);
	float view_scale = camera.calculate_view_scale(this->scale_);

	sprite_font->DrawString(
		sprite_batch,
		this->text_.c_str(),
		view_pos.get_xm_vector(),
		this->get_colour().get_xm_vector(),
		this->get_draw_rotation(),
		this->get_origin().get_xm_vector(),
		view_scale,
		this->get_effects(),
		this->get_layer_depth());
}

void TextObject::draw(SpriteBatch* sprite_batch) const
{
	const SpriteFont* sprite_font =
		this->get_render_resources()->get_sprite_font(this->font_);

	sprite_font->DrawString(
		sprite_batch,
		this->text_.c_str(),
		this->position_.get_xm_vector(),
		this->get_colour().get_xm_vector(),
		this->get_draw_rotation(),
		this->get_origin().get_xm_vector(),
		this->scale_,
		this->get_effects(),
		this->get_layer_depth());
}

void TextObject::draw_with(SpriteBatch* sprite_batch,
	const Camera& camera,
	const Colour& colour,
	const Vector2F& position,
	float scale) const
{
	const SpriteFont* sprite_font =
		this->get_render_resources()->get_sprite_font(this->font_);

	sprite_font->DrawString(
		sprite_batch,
		this->text_.c_str(),
		camera.calculate_view_position(position).get_xm_vector(),
		colour.get_xm_vector(),
		this->get_draw_rotation(),
		this->get_origin().get_xm_vector(),
		camera.calculate_view_scale(scale),
		this->get_effects(),
		this->get_layer_depth());
}

const std::string& TextObject::get_text() const
{
	return this->text_;
}
RenderResources::FontHandle TextObject::get_font() const
{
	return this->font_;
}
const Vector2F& TextObject::get_position() const
{
	return this->position_;
}
float TextObject::get_scale() const
{
	return this->scale_;
}
void TextObject::set_text(const std::string& text)
{
	this->text_ = text;
}
void TextObject::set_font(const std::string& font_name)
{
	this->font_ = this->get_render_resources()->resolve_sprite_font(font_name);
}
void TextObject::set_position(const Vector2F& position)
{
	this->position_ = position;
}
void TextObject::set_scale(float scale)
{
	this->scale_ = scale;
}
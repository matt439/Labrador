#include "engine/render/text_object.h"

using namespace DirectX;
using namespace mattmath;

namespace artattack
{
	TextObject::TextObject(const std::wstring& text,
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
		this->remeasure();
	}

	void TextObject::draw(SpriteBatch* sprite_batch, const Camera& camera) const
	{
		const SpriteFont* sprite_font =
			this->render_resources()->sprite_font(this->font_);

		Vector2F view_pos = camera.calculate_view_position(this->position_);
		float view_scale = camera.calculate_view_scale(this->scale_);

		sprite_font->DrawString(
			sprite_batch,
			this->text_.c_str(),
			view_pos.xm_vector(),
			this->colour().xm_vector(),
			this->draw_rotation(),
			this->origin().xm_vector(),
			view_scale,
			this->effects(),
			this->layer_depth());
	}

	void TextObject::draw(SpriteBatch* sprite_batch) const
	{
		const SpriteFont* sprite_font =
			this->render_resources()->sprite_font(this->font_);

		sprite_font->DrawString(
			sprite_batch,
			this->text_.c_str(),
			this->position_.xm_vector(),
			this->colour().xm_vector(),
			this->draw_rotation(),
			this->origin().xm_vector(),
			this->scale_,
			this->effects(),
			this->layer_depth());
	}

	void TextObject::draw_with(SpriteBatch* sprite_batch,
		const Camera& camera,
		const Colour& colour,
		const Vector2F& position,
		float scale) const
	{
		const SpriteFont* sprite_font =
			this->render_resources()->sprite_font(this->font_);

		sprite_font->DrawString(
			sprite_batch,
			this->text_.c_str(),
			camera.calculate_view_position(position).xm_vector(),
			colour.xm_vector(),
			this->draw_rotation(),
			this->origin().xm_vector(),
			camera.calculate_view_scale(scale),
			this->effects(),
			this->layer_depth());
	}

	const std::wstring& TextObject::text() const
	{
		return this->text_;
	}
	RenderResources::FontHandle TextObject::font() const
	{
		return this->font_;
	}
	const Vector2F& TextObject::position() const
	{
		return this->position_;
	}
	float TextObject::scale() const
	{
		return this->scale_;
	}
	void TextObject::set_text(const std::wstring& text)
	{
		this->text_ = text;
		this->remeasure();
	}
	void TextObject::set_font(const std::string& font_name)
	{
		this->font_ = this->render_resources()->resolve_sprite_font(font_name);
		this->remeasure();
	}
	void TextObject::remeasure()
	{
		const SpriteFont* sprite_font =
			this->render_resources()->sprite_font(this->font_);
		const XMVECTOR size = sprite_font->MeasureString(this->text_.c_str());
		this->measured_size_ =
			Vector2F(XMVectorGetX(size), XMVectorGetY(size));
	}
	RectangleF TextObject::text_bounds() const
	{
		return this->text_bounds_at(this->position_, this->scale_);
	}
	RectangleF TextObject::text_bounds_at(const Vector2F& position,
		float scale) const
	{
		const Vector2F size = this->measured_size_ * scale;
		const Vector2F top_left = position - this->origin() * scale;
		return { top_left, size };
	}
	void TextObject::set_position(const Vector2F& position)
	{
		this->position_ = position;
	}
	void TextObject::set_scale(float scale)
	{
		this->scale_ = scale;
	}
}
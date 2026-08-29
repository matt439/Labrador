#include "engine/render/text_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <string>

using namespace mattmath;

namespace labrador
{
	TextObject::TextObject(const std::wstring& text,
		const std::string& font_name,
		const Vector2F& position,
		RenderResources* render_resources,
		const Colour& color,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth) :
		DrawObject(render_resources, color,
			rotation, origin, SpriteFlip::none, layer_depth),
		text_(text),
		font_(render_resources->resolve_sprite_font(font_name)),
		position_(position),
		scale_(scale)
	{
		this->remeasure();
	}

	void TextObject::draw(DrawList& draw_list) const
	{
		draw_list.draw_text(this->font_,
			this->text_,
			this->position_,
			this->colour(),
			this->scale_,
			this->draw_rotation(),
			this->origin(),
			this->layer_depth());
	}

	void TextObject::draw_with(DrawList& draw_list,
		const Colour& colour,
		const Vector2F& position,
		float scale) const
	{
		draw_list.draw_text(this->font_,
			this->text_,
			position,
			colour,
			scale,
			this->draw_rotation(),
			this->origin(),
			this->layer_depth());
	}

	const std::wstring& TextObject::text() const
	{
		return this->text_;
	}
	FontHandle TextObject::font() const
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
		// The font table measures, because the font table is the only thing
		// that has the font. NO BACKEND ANSWERS IT: RenderResources::measure_
		// text is compiled once, engine-side, and forwards to Font::measure -
		// so what makes a TextObject constructible without a device is that
		// this call never reaches a backend at all - which is stronger than any
		// claim about what a backend would answer.
		this->measured_size_ =
			this->render_resources()->measure_text(this->font_, this->text_);
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

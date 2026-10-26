#include "engine/render/text_object.h"

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/font.h"
#include "engine/render/sprite_geometry.h"

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
		// Layout keeps the line spacing; culling also needs cells whose
		// bearings or whitespace metrics put ink outside that rectangle.
		// Both walks are eager and shared with drawing, with no backend call
		// and no font pointer retained across device loss.
		const Font* loaded_font = this->render_resources()->font(this->font_);
		const Vector2F layout_size = loaded_font->measure(this->text_);
		RectangleF measured_bounds(Vector2F::ZERO, layout_size);
		loaded_font->for_each_glyph(this->text_,
			[&measured_bounds](const Glyph& glyph, const Vector2F& pen)
			{
				const RectangleF cell(pen.x, pen.y + glyph.y_offset,
					static_cast<float>(glyph.subrect.width),
					static_cast<float>(glyph.subrect.height));
				measured_bounds = RectangleF::union_of(measured_bounds, cell);
			});
		this->measured_bounds_ = measured_bounds;
	}
	RectangleF TextObject::text_bounds() const
	{
		return this->text_bounds_at(this->position_, this->scale_);
	}
	RectangleF TextObject::text_bounds_at(const Vector2F& position,
		float scale) const
	{
		return text_quad_bounds(position, scale, this->measured_bounds_.size(),
			this->draw_rotation(), this->origin() -
			this->measured_bounds_.position());
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

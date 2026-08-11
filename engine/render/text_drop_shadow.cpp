#include "engine/render/text_drop_shadow.h"
#include "engine/math/vector2f.h"

#include <string>

using namespace mattmath;

namespace artattack
{
	TextDropShadow::TextDropShadow(const std::wstring& text,
		const std::string& font_name,
		const Vector2F& position,
		RenderResources* render_resources,
		const Colour& color,
		const Colour& shadow_color,
		const Vector2F& shadow_offset,
		float scale,
		float shadow_scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth) :
		Text(text, font_name, position, render_resources,
		     color, scale, rotation, origin, layer_depth),
		shadow_offset_(shadow_offset),
		shadow_color_(shadow_color),
		shadow_scale_(shadow_scale)
	{

	}
	void TextDropShadow::draw(DrawList& draw_list) const
	{
		// Pass the shadow's colour, offset position and scale rather than assigning
		// them to this object and restoring them afterwards. The save/restore was a
		// data race: the render workers all run draw() on the same object, so one
		// thread could restore the originals while another was mid-shadow.
		this->TextObject::draw_with(draw_list,
			this->shadow_color_,
			this->position() + this->shadow_offset_,
			this->shadow_scale_);

		this->TextObject::draw(draw_list);
	}
	Vector2F TextDropShadow::shadow_offset() const
	{
		return this->shadow_offset_;
	}
	Colour TextDropShadow::shadow_color() const
	{
		return this->shadow_color_;
	}
	float TextDropShadow::shadow_scale() const
	{
		return this->shadow_scale_;
	}
	void TextDropShadow::set_shadow_offset(const Vector2F& offset)
	{
		this->shadow_offset_ = offset;
	}
	void TextDropShadow::set_shadow_color(const Colour& color)
	{
		this->shadow_color_ = color;
	}
	void TextDropShadow::set_shadow_scale(float scale)
	{
		this->shadow_scale_ = scale;
	}
}

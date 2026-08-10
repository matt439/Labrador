#include "engine/render/text.h"

#include <string>

using namespace mattmath;

namespace artattack
{
	Text::Text(const std::wstring& text,
		const std::string& font_name,
		const Vector2F& position,
		RenderResources* render_resources,
		const Colour& color,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth) :
		TextObject(text, font_name, position, render_resources,
			color, scale, rotation, origin, layer_depth)
	{

	}
	void Text::set_text(const std::wstring& text)
	{
		this->TextObject::set_text(text);
	}
	void Text::set_colour(const Colour& colour)
	{
		this->TextObject::set_colour(colour);
	}
	void Text::set_scale(float scale)
	{
		this->TextObject::set_scale(scale);
	}
	void Text::set_position(const Vector2F& position)
	{
		this->TextObject::set_position(position);
	}
}

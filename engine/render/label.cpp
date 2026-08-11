#include "engine/render/label.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <string>

using namespace mattmath;

namespace artattack
{
	Label::Label(const std::wstring& text,
		const std::string& font_name,
		const Vector2F& position,
		RenderResources* render_resources,
		const Colour& colour,
		float scale,
		float rotation,
		const Vector2F& origin,
		float layer_depth) :
		Text(text, font_name, position, render_resources, colour, scale,
			rotation, origin, layer_depth)
	{
	}

	void Label::update(float /*dt*/)
	{
	}

	void Label::draw(DrawList& draw_list) const
	{
		this->TextObject::draw(draw_list);
	}

	RectangleF Label::bounds() const
	{
		return this->text_bounds();
	}
}

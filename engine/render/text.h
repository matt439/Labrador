#pragma once

#include "engine/render/text_object.h"
#include "engine/render/colour.h"
#include "engine/math/vector2f.h"

#include <string>

namespace labrador
{
	// A drawable string, without a scene's GameObject interface. Use Label
	// when a scene should update, draw and cull the string.
	class Text : public TextObject
	{
	public:
		Text() = default;

		// `font_name` names a loaded font in `render_resources`, normally
		// Application::render_resources(). That table is borrowed and must
		// outlive this object. An unknown font name throws std::out_of_range.
		//
		// `position` locates the text's origin in world coordinates. `origin`
		// is the offset from the layout's top-left in unscaled font texels;
		// its default zero anchors that corner at `position`. Glyph bearings
		// can offset the visible ink. `scale` scales the font's pixels, and
		// `rotation` is in radians, clockwise on screen.
		Text(const std::wstring& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float layer_depth = 0.0f);

		// What the string says and what colour it is drawn in, readable by
		// anyone who can set them. A widget built from text - a settings row
		// whose value changes, a focus group's paint - has to be checkable
		// from outside, and without these its tests could only infer the
		// words from the box they measure to.
		using TextObject::text;
		using DrawObject::colour;

		void set_text(const std::wstring& text) override;
		void set_colour(const Colour& colour) override;
		void set_scale(float scale) override;
		void set_position(const mattmath::Vector2F& position) override;
	};
}

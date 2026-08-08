#pragma once

#include "engine/render/text_object.h"
#include "engine/render/colour.h"

namespace artattack
{
	class Text : public TextObject
	{
	public:
		Text() = default;
		Text(const std::wstring& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float layer_depth = 0.0f);

		void set_text(const std::wstring& text) override;
		void set_colour(const Colour& colour) override;
		void set_scale(float scale) override;
		void set_position(const mattmath::Vector2F& position) override;
	};
}

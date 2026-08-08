#pragma once

#include "engine/render/text.h"

namespace artattack
{
	class TextDropShadow : public Text
	{
	public:
		TextDropShadow() = default;
		TextDropShadow(const std::wstring& text,
					const std::string& font_name,
					const mattmath::Vector2F& position,
					RenderResources* render_resources,
					const mattmath::Colour& color = colour_consts::WHITE,
					const mattmath::Colour& shadow_color = colour_consts::BLACK,
					const mattmath::Vector2F& shadow_offset = { 2.0f, 2.0f },
					float scale = 1.0f,
					float shadow_scale = 1.0f,
					float rotation = 0.0f,
					const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
					float layer_depth = 0.0f);

		// const, like every draw below it - and here const also buys correctness
		// rather than only safety. While this was non-const it did not override
		// TextObject::draw(...) const at all, it *hid* it, so a TextDropShadow
		// reached through a Text& drew its text with no shadow.
		void draw(DrawList& draw_list) const override;

		mattmath::Vector2F shadow_offset() const;
		mattmath::Colour shadow_color() const;
		float shadow_scale() const;

		void set_shadow_offset(const mattmath::Vector2F& offset);
		void set_shadow_color(const mattmath::Colour& color);
		void set_shadow_scale(float scale);

	private:
		mattmath::Vector2F shadow_offset_ = { 2.0f, 2.0f };
		mattmath::Colour shadow_color_ = colour_consts::BLACK;
		float shadow_scale_ = 1.0f;
	};
}

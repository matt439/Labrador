#pragma once

#include "engine/render/text_object.h"

class Text : public TextObject
{
public:
	Text() = default;
	Text(const std::string& text,
		const std::string& font_name,
		const mattmath::Vector2F& position,
		RenderResources* render_resources,
		const mattmath::Colour& color = colour_consts::WHITE,
		float scale = 1.0f,
		float rotation = 0.0f,
		const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

	void set_text(const std::string& text) override;
	void set_colour(const mattmath::Colour& colour) override;
	void set_scale(float scale) override;
	void set_position(const mattmath::Vector2F& position) override;
};

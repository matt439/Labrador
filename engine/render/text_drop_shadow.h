#pragma once

#include "engine/render/text.h"

class TextDropShadow : public Text
{
public:
	TextDropShadow() = default;
	TextDropShadow(const std::string& text,
				const std::string& font_name,
				const MattMath::Vector2F& position,
				RenderResources* render_resources,
				const MattMath::Colour& color = colour_consts::WHITE,
				const MattMath::Colour& shadow_color = colour_consts::BLACK,
				const MattMath::Vector2F& shadow_offset = { 2.0f, 2.0f },
				float scale = 1.0f,
				float shadow_scale = 1.0f,
				float rotation = 0.0f,
				const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
				DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
				float layer_depth = 0.0f);

	virtual void draw(DirectX::SpriteBatch* sprite_batc, const MattMath::Camera& camera);
	virtual void draw(DirectX::SpriteBatch* sprite_batc);

	MattMath::Vector2F get_shadow_offset() const;
	MattMath::Colour get_shadow_color() const;
	float get_shadow_scale() const;

	void set_shadow_offset(const MattMath::Vector2F& offset);
	void set_shadow_color(const MattMath::Colour& color);
	void set_shadow_scale(float scale);

private:
	MattMath::Vector2F shadow_offset_ = { 2.0f, 2.0f };
	MattMath::Colour shadow_color_ = colour_consts::BLACK;
	float shadow_scale_ = 1.0f;
};

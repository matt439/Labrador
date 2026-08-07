#pragma once

#include "engine/render/draw_object.h"

// A string drawn in one font.
//
// The font name is resolved to a handle at construction. A handle rather than
// a cached SpriteFont*, because fonts are device resources: a device loss
// destroys and rebuilds every one of them, and a pointer taken before the loss
// would be dangling after it. The handle names the registry slot, and the
// reload refills that same slot.
class TextObject : public DrawObject
{
public:
	TextObject() = default;
	TextObject(const std::string& text,
		const std::string& font_name,
		const MattMath::Vector2F& position,
		RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float scale = 1.0f,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

	virtual void draw(DirectX::SpriteBatch* sprite_batch,
		const MattMath::Camera& camera) const;
	virtual void draw(DirectX::SpriteBatch* sprite_batch) const;

	// Draws with the given colour, position and scale without storing any of
	// them. TextDropShadow used to draw its shadow by assigning those three
	// members, drawing, and assigning them back - a save/restore that is a
	// data race the moment two render workers run it on the same object.
	void draw_with(DirectX::SpriteBatch* sprite_batch,
		const MattMath::Camera& camera,
		const MattMath::Colour& colour,
		const MattMath::Vector2F& position,
		float scale) const;

protected:
	const std::string& get_text() const;
	RenderResources::FontHandle get_font() const;
	const MattMath::Vector2F& get_position() const;
	float get_scale() const;

	virtual void set_text(const std::string& text);
	void set_font(const std::string& font_name);
	virtual void set_position(const MattMath::Vector2F& position);
	virtual void set_scale(float scale);
private:
	std::string text_ = "";
	RenderResources::FontHandle font_;
	MattMath::Vector2F position_ = MattMath::Vector2F::ZERO;
	float scale_ = 1.0f;
};

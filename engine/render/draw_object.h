#pragma once

#include "engine/math/colour.h"
#include "engine/render/render_resources.h"

class DrawObject
{
public:
	virtual ~DrawObject() = default;
	DrawObject() = default;
	DrawObject(RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

protected:
	virtual RenderResources* get_render_resources() const;
	virtual const MattMath::Colour& get_colour() const;
	virtual float get_draw_rotation() const;
	virtual const MattMath::Vector2F& get_origin() const;
	virtual DirectX::SpriteEffects get_effects() const;
	virtual float get_layer_depth() const;

	virtual void set_colour(const MattMath::Colour& colour);
	virtual void set_draw_rotation(float rotation);
	virtual void set_origin(const MattMath::Vector2F& origin);
	virtual void set_effects(DirectX::SpriteEffects effects);
	virtual void set_layer_depth(float layer_depth);

	void set_draw_rotation_by_rectangle_rotated(const MattMath::RectangleRotated& rect);

private:
	RenderResources* render_resources_ = nullptr;
	MattMath::Colour colour_ = colour_consts::WHITE;
	float draw_rotation_ = 0.0f;
	MattMath::Vector2F origin_ = MattMath::Vector2F::ZERO;
	DirectX::SpriteEffects effects_ = DirectX::SpriteEffects_None;
	float layer_depth_ = 0.0f;
};

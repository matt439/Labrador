#pragma once

#include "engine/math/colour.h"
#include "engine/render/render_resources.h"

namespace artattack
{
	class DrawObject
	{
	public:
		virtual ~DrawObject() = default;
		DrawObject() = default;
		DrawObject(RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

	protected:
		virtual RenderResources* render_resources() const;
		virtual const mattmath::Colour& colour() const;
		virtual float draw_rotation() const;
		virtual const mattmath::Vector2F& origin() const;
		virtual SpriteFlip flip() const;
		virtual float layer_depth() const;

		virtual void set_colour(const mattmath::Colour& colour);
		virtual void set_draw_rotation(float rotation);
		virtual void set_origin(const mattmath::Vector2F& origin);
		virtual void set_flip(SpriteFlip flip);
		virtual void set_layer_depth(float layer_depth);

		void set_draw_rotation_by_rectangle_rotated(const mattmath::RectangleRotated& rect);

	private:
		RenderResources* render_resources_ = nullptr;
		mattmath::Colour colour_ = colour_consts::WHITE;
		float draw_rotation_ = 0.0f;
		mattmath::Vector2F origin_ = mattmath::Vector2F::ZERO;
		SpriteFlip flip_ = SpriteFlip::none;
		float layer_depth_ = 0.0f;
	};
}

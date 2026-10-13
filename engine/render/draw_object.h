#pragma once

#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/vector2f.h"

namespace labrador
{
	class DrawObject
	{
	public:
		virtual ~DrawObject() = default;
		DrawObject() = default;
		DrawObject(RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

	protected:
		virtual RenderResources* render_resources() const;
		virtual const Colour& colour() const;
		virtual float draw_rotation() const;
		virtual const mattmath::Vector2F& origin() const;
		virtual SpriteFlip flip() const;

		// TextureObject, AnimationObject and TextObject pass this to every draw
		// they make, and no backend reads it: what is on top is decided by the order of the draw calls into a list
		// (DrawList::draw_sprite in engine/render/renderer.h), never by depth.
		// A Scene draws its plain objects in the order they were added and its
		// collision objects after them, in no order it promises.
		virtual float layer_depth() const;

		virtual void set_colour(const Colour& colour);
		virtual void set_draw_rotation(float rotation);
		virtual void set_origin(const mattmath::Vector2F& origin);
		virtual void set_flip(SpriteFlip flip);
		virtual void set_layer_depth(float layer_depth);

		void set_draw_rotation_by_rectangle_rotated(const mattmath::RectangleRotated& rect);

	private:
		RenderResources* render_resources_ = nullptr;
		Colour colour_ = Colour::white;
		float draw_rotation_ = 0.0f;
		mattmath::Vector2F origin_ = mattmath::Vector2F::ZERO;
		SpriteFlip flip_ = SpriteFlip::none;
		float layer_depth_ = 0.0f;
	};
}

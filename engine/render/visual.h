#pragma once

#include "engine/core/game_object.h"
#include "engine/render/texture_object.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"

#include <string>

namespace labrador
{
	class Visual final : public GameObject, public TextureObject
	{
	public:
		Visual() = default;
		Visual(const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleF& rectangle,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		Visual(const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleRotated& rect_rotated,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;

		// The box the sprite is drawn into, in world space: the destination
		// rectangle with the frame's authored origin, the caller's origin and
		// the rotation applied, exactly as draw() applies them
		// (sprite_geometry.h, sprite_quad_bounds). It was the destination
		// rectangle alone, which is the drawn extent only for a sprite with
		// no origin and no turn, and a scene culling against it dropped the
		// others at the edge of every view.
		//
		// COMPUTED ONCE, IN THE CONSTRUCTOR, and that is safe because nothing
		// it reads can change afterwards: this class is final, rectangle_ is
		// set nowhere but the two constructors, and the frame, origin and
		// rotation setters it inherits are protected. That is what keeps the
		// cull path - per object, per view, per frame - to one return, with
		// no sheet lookup and no trigonometry on it. A setter added to this
		// class has to recompute it, and that sentence is the whole of the
		// contract.
		mattmath::RectangleF bounds() const override;

	protected:
		mattmath::RectangleF rectangle_ = mattmath::RectangleF::ZERO;

	private:
		mattmath::RectangleF bounds_ = mattmath::RectangleF::ZERO;

		// From rectangle_ and the frame, once both are set.
		mattmath::RectangleF drawn_bounds() const;
	};
}

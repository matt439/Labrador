#pragma once

#include "engine/core/game_object.h"
#include "engine/render/texture_object.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"

#include <string>

namespace artattack
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

		mattmath::RectangleF bounds() const override;

	protected:
		mattmath::RectangleF rectangle_ = mattmath::RectangleF::ZERO;
	};
}

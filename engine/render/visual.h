#pragma once

#include "engine/core/i_game_object.h"
#include "engine/render/texture_object.h"
#include "engine/math/matt_math.h"

namespace artattack
{
	class Visual final : public IGameObject, public TextureObject
	{
	public:
		Visual() = default;
		Visual(const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleF& rectangle,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);

		Visual(const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleRotated& rect_rotated,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DirectX::SpriteBatch* sprite_batch,
					const mattmath::Camera& camera) const override;

		mattmath::RectangleF bounds() const override;

	protected:
		mattmath::RectangleF rectangle_ = mattmath::RectangleF::ZERO;
	};
}

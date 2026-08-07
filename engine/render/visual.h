#pragma once

#include "engine/core/i_game_object.h"
#include "engine/render/texture_object.h"
#include "engine/math/matt_math.h"

class Visual final : public IGameObject, public TextureObject
{
public:
	Visual() = default;
	Visual(const std::string& sheet_name,
		const std::string& frame_name,
		const MattMath::RectangleF& rectangle,
		RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

	Visual(const std::string& sheet_name,
		const std::string& frame_name,
		const MattMath::RectangleRotated& rect_rotated,
		RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);

	void update() override;
	void draw(DirectX::SpriteBatch* sprite_batch,
				const MattMath::Camera& camera) override;
	void draw(DirectX::SpriteBatch* sprite_batch) override;

	bool is_visible_in_viewport(const MattMath::RectangleF& view) const override;

protected:
	MattMath::RectangleF rectangle_ = MattMath::RectangleF::ZERO;
};

#pragma once

#include "engine/core/game_object.h"
#include "engine/render/renderer.h"

#include <array>
#include <cstddef>

namespace multiplayer
{
	// One world, shared by both views. The scene owns it; update changes it
	// once per tick, and each view's worker only reads it.
	class Arena final : public labrador::GameObject
	{
	public:
		static constexpr size_t player_count = 2;

		explicit Arena(labrador::TextureHandle white);
		void set_directions(const std::array<mattmath::Vector2F, player_count>& directions);
		void update(float dt) override;
		void draw(labrador::DrawList& list) const override;
		mattmath::RectangleF bounds() const override;

		mattmath::Vector2F position(size_t player) const;
		labrador::Camera camera(size_t player, const labrador::Viewport& viewport) const;
		static labrador::Colour player_colour(size_t player);

	private:
		labrador::TextureHandle white_;
		std::array<mattmath::Vector2F, player_count> positions_ = {
			mattmath::Vector2F(720.0f, 540.0f), mattmath::Vector2F(960.0f, 540.0f)
		};
		std::array<mattmath::Vector2F, player_count> directions_ = {};
	};
}

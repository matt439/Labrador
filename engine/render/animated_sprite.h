#pragma once

#include "engine/render/sprite_sheet.h"
#include "SpriteBatch.h"

namespace artattack
{
	class AnimatedSprite
	{
	public:
		AnimatedSprite(SpriteSheet* sprite_sheet,
			const std::string& animation_strip_name,
			const float* dt);

		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::RectangleI& destination_rectangle,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f) const;

		void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Vector2F& position,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float scale = 1.0f,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f) const;
		void update();
		void reset();
		void stop();
		void play();
		void pause();

		bool is_paused() const;

		void set_animation_strip(SpriteSheet* sprite_sheet,
			const std::string& animation_strip_name);
		void set_frame_index(int frame_index);

	private:
		const float* dt_ = nullptr;
		SpriteSheet* sprite_sheet_ = nullptr;
		const AnimationStrip* animation_strip_ = nullptr;
		int frame_index_ = 0;
		bool paused_ = false;
		float time_elapsed_ = 0.0f;
	};
}

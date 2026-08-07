#pragma once

#include "engine/render/sprite_sheet_object.h"
#include "engine/render/animated_sprite.h"

namespace artattack
{
	// A playing animation out of a sprite sheet.
	//
	// Like TextureObject, the element name is resolved once and kept as a handle -
	// and it matters more here, because the strip is read on the update path as
	// well as the draw path: once for the frame advance, again for the source
	// rectangle, every frame, for every animated object in the level.
	class AnimationObject : public SpriteSheetObject
	{
	public:
		AnimationObject() = default;
		AnimationObject(const float* dt,
			const std::string& sheet_name,
			const std::string& animation_strip_name,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);
	protected:
		virtual void update();
		void reset();
		void stop();
		void play();
		void pause();
		void set_frame_time(float frame_time);
		void set_frame_time_to_default();
		bool is_paused() const;

		void set_frame_index(int frame_index);

		// Moves sheet and strip together - a strip handle resolved against one
		// sheet indexes nothing meaningful in another.
		void set_animation_strip_and_reset(const std::string& sprite_sheet,
			const std::string& animation_strip);

		virtual void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::RectangleI& destination_rectangle) const;
		virtual void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::RectangleF& destination_rectangle) const;
		virtual void draw(DirectX::SpriteBatch* sprite_batch, const mattmath::Vector2F& position,
			float scale = 1.0f) const;

		virtual void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::RectangleF& destination_rectangle,
			const mattmath::Camera& camera) const;
		virtual void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Vector2F& position,
			const mattmath::Camera& camera, float scale = 1.0f) const;

		// See TextureObject::draw_with - lets a caller vary colour and effects per
		// draw without assigning them to this shared object first.
		void draw_with(DirectX::SpriteBatch* sprite_batch,
			const mattmath::RectangleF& destination_rectangle,
			const mattmath::Camera& camera,
			const mattmath::Colour& colour,
			DirectX::SpriteEffects effects) const;

	private:
		const float* dt_ = nullptr;
		SpriteSheet::strip_handle strip_;
		int frame_index_ = 0;
		bool paused_ = false;
		float time_elapsed_ = 0.0f;
		float frame_time_ = 0.0f;
		const AnimationStrip& animation_strip() const;
		const RECT* source_rectangle() const;
	};
}

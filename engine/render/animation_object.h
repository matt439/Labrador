#pragma once

#include "engine/render/sprite_sheet_object.h"

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
		AnimationObject(const std::string& sheet_name,
			const std::string& animation_strip_name,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);
	protected:
		virtual void update(float dt);
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

		// No camera parameter; see TextureObject.
		virtual void draw(DrawList& draw_list,
			const mattmath::RectangleF& destination_rectangle) const;
		virtual void draw(DrawList& draw_list,
			const mattmath::Vector2F& position, float scale = 1.0f) const;

		// See TextureObject::draw_with - lets a caller vary colour and flip per
		// draw without assigning them to this shared object first.
		void draw_with(DrawList& draw_list,
			const mattmath::RectangleF& destination_rectangle,
			const mattmath::Colour& colour,
			SpriteFlip flip) const;

	private:
		SpriteSheet::strip_handle strip_;
		int frame_index_ = 0;
		bool paused_ = false;
		float time_elapsed_ = 0.0f;
		float frame_time_ = 0.0f;
		const AnimationStrip& animation_strip() const;
		const mattmath::RectangleI& source_rectangle() const;
	};
}

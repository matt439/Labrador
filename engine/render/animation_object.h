#pragma once

#include "engine/render/sprite_sheet_object.h"
#include "engine/render/colour.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <string>

namespace labrador
{
	// A playing animation out of a sprite sheet.
	//
	// Like TextureObject, the element name is resolved once and kept as a handle -
	// and it matters more here, because the strip is read on the update path as
	// well as the draw path: once for the frame advance, again for the source
	// rectangle, every frame, for every animated object in the scene.
	//
	// A strip can also arrive resolved, as a RenderResources::SheetStrip, which
	// is the form for per-frame code - see TextureObject. A pair is meaningful
	// only against the RenderResources this object was built on.
	class AnimationObject : public SpriteSheetObject
	{
	public:
		AnimationObject() = default;
		AnimationObject(const std::string& sheet_name,
			const std::string& animation_strip_name,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		// The same, from a pair resolved at load. Throws std::out_of_range if
		// the pair is unresolved.
		AnimationObject(RenderResources::SheetStrip strip,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
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
		// sheet indexes nothing meaningful in another - then takes the strip's
		// frame time and resets the clock. The pair form resolves nothing and
		// is the one for per-frame code; it throws std::out_of_range, at the
		// call, if the pair is unresolved or names a slot this object's table
		// does not have. The name form resolves first and assigns after, so a
		// name that does not resolve throws and leaves the object as it was.
		void set_animation_strip_and_reset(RenderResources::SheetStrip strip);
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
			const Colour& colour,
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

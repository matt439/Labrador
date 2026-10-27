#pragma once

#include "engine/render/draw_object.h"
#include "engine/render/colour.h"
#include "engine/math/vector2f.h"

#include <string>

namespace labrador
{
	// A drawable that draws out of one sprite sheet.
	//
	// It holds a handle to that sheet, not its name. The name is resolved once,
	// here in the constructor, because holding the name means a
	// std::map<std::string, ...> descent per draw, per drawable, from every
	// render worker at once (PHILOSOPHY T7, T8).
	//
	// What a sheet *element* is depends on the subclass - a frame for
	// TextureObject - so the element handle lives down there. The sheet lives
	// up here because every subclass needs it and
	// because an element handle only means anything against the sheet it was
	// resolved from. Those two therefore change together: see set_sprite_sheet.
	class SpriteSheetObject : public DrawObject
	{
	public:
		SpriteSheetObject() = default;
		SpriteSheetObject(const std::string& sheet_name,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);
	protected:
		// A sheet already resolved, for a subclass constructor that takes a
		// paired handle (RenderResources::SheetFrame, SheetStrip) and so has
		// no name to resolve. The handle must come from `render_resources`.
		SpriteSheetObject(RenderResources::SpriteSheetHandle sheet,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		const SpriteSheet* sprite_sheet() const;

		// Re-points this object at another sheet. Every element handle a subclass
		// is holding refers to the old sheet and is meaningless against the new
		// one, so a subclass calls either only from a setter that moves its
		// element in the same breath - never on its own. The handle form
		// resolves nothing, and its handle must come from this object's own
		// RenderResources.
		void set_sprite_sheet(const std::string& sheet_name);
		void set_sprite_sheet(RenderResources::SpriteSheetHandle sheet);
	private:
		RenderResources::SpriteSheetHandle sheet_;
	};
}

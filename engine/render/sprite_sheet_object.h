#pragma once

#include "engine/render/draw_object.h"

// A drawable that draws out of one sprite sheet.
//
// It holds a handle to that sheet, not its name. The name is resolved once,
// here in the constructor, because the alternative is what this class used to
// do: a std::map<std::string, ...> descent per draw, per drawable, from every
// render worker at once (PHILOSOPHY T7, T8).
//
// What a sheet *element* is depends on the subclass - a frame for
// TextureObject, an animation strip for AnimationObject - so the element
// handle lives down there. The sheet lives up here because both need it and
// because an element handle only means anything against the sheet it was
// resolved from. Those two therefore change together: see set_sprite_sheet.
class SpriteSheetObject : public DrawObject
{
public:
	SpriteSheetObject() = default;
	SpriteSheetObject(const std::string& sheet_name,
		RenderResources* render_resources,
		const MattMath::Colour& color = colour_consts::WHITE,
		float rotation = 0.0f,
		const MattMath::Vector2F& origin = MattMath::Vector2F::ZERO,
		DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
		float layer_depth = 0.0f);
protected:
	SpriteSheet* get_sprite_sheet() const;

	// Re-points this object at another sheet. Every element handle a subclass
	// is holding refers to the old sheet and is meaningless against the new
	// one, so a subclass calls this only from a setter that re-resolves its
	// element in the same breath - never on its own.
	void set_sprite_sheet(const std::string& sheet_name);
private:
	RenderResources::SpriteSheetHandle sheet_;
};

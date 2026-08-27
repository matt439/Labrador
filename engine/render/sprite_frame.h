#pragma once

#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

namespace labrador
{
	// One named rectangle inside a sprite sheet, and the pivot the sheet
	// authored for it.
	//
	// THERE IS NO `rotated` HERE, AND THERE WAS. A packer can store a frame
	// turned ninety degrees to pack it tighter, and the sheet schema has always
	// had a key saying so; this class stored the answer in a bool that had no
	// accessor and that nothing on any draw path ever read. The sheet said
	// turned, the engine drew upright, and the disagreement was invisible.
	// engine/assets/sprite_sheet_loader.cpp refuses such a frame by name now,
	// which is the whole of the engine's position on packer rotation, and the
	// member went with it: a capability the engine does not have should not
	// have somewhere to be stored, or the next person to want it finds the
	// field already there and fills it in rather than arguing for the feature.
	class SpriteFrame
	{
	public:
		SpriteFrame() = default;
		explicit SpriteFrame(const mattmath::RectangleI& source_rectangle,
		                     const mattmath::Vector2F& origin =
			                     mattmath::Vector2F::ZERO);
		SpriteFrame(const mattmath::Vector2F& position,
			const mattmath::Vector2F& size,
			const mattmath::Vector2F& origin =
				mattmath::Vector2F::ZERO);

		// A RectangleI rather than the RECT* this used to hand out. The RECT
		// was here only because SpriteBatch::Draw wanted one; the frame kept
		// both forms of the same rectangle so it had a stable address to
		// return, and the two could drift. The seam takes a RectangleI, so the
		// duplicate is gone and the conversion happens once, in the backend.
		const mattmath::RectangleI& source_rectangle() const;

		// The pivot the sheet authored, in unscaled source texels measured from
		// this frame's top left - the seam's own units for an origin
		// (engine/render/sprite_geometry.h), so nothing converts on the way to
		// a quad. Zero for a frame whose definition does not mention one, which
		// is most of them and is why the default draws from the top left.
		//
		// It is not a term of the draw on its own: SpriteSheet::draw adds it to
		// whatever origin the caller passed, and that file says why the two
		// compose rather than one replacing the other.
		const mattmath::Vector2F& origin() const;
	private:
		mattmath::RectangleI source_rectangle_ = mattmath::RectangleI::ZERO;
		mattmath::Vector2F origin_ = mattmath::Vector2F::ZERO;
	};
}

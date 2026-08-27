#pragma once

#include "engine/core/name_table.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/animation_strip.h"
#include "engine/render/colour.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include <string>

namespace labrador
{
	// One texture, and the names of the rectangles inside it.
	//
	// The texture is a handle, not a pointer. It used to be a raw
	// ID3D11ShaderResourceView* that a device restore had to re-seat by hand -
	// which is what set_texture() was for, and why the sprite-sheet asset kind
	// needed a reload function distinct from its load function. A handle names a
	// registry slot and the reload refills that slot, so the sheet survives a
	// device loss without being touched, and this class stops naming a backend
	// type at all.
	class SpriteSheet
	{
	public:
		// What a frame name and a strip name become once resolved. They are
		// distinct types on purpose: both are an index into this sheet, and the
		// two tables are not the same table.
		using frame_handle = Handle<SpriteFrame>;
		using strip_handle = Handle<AnimationStrip>;

		// Built by read_sprite_sheet (engine/assets/) from an already-parsed
		// definition: the sheet indexes and draws, it does not read files.
		SpriteSheet(TextureHandle texture,
			NameTable<SpriteFrame> sprite_frames,
			NameTable<AnimationStrip> animation_strips);

		// Load-time. Turn a name from a definition file into something the draw
		// path can carry. Both throw std::out_of_range naming the element if this
		// sheet does not contain it.
		//
		// A handle is an index into *this* sheet and means nothing against
		// another, so whatever holds one holds the sheet it came from too.
		frame_handle resolve_sprite_frame(const std::string& name) const;
		strip_handle resolve_animation_strip(const std::string& name) const;

		// Per-frame. No name, no map: an index into a contiguous table.
		const SpriteFrame& sprite_frame(frame_handle frame) const;
		const AnimationStrip& animation_strip(strip_handle strip) const;

		TextureHandle texture() const;

		// Every draw is const: a single SpriteSheet is shared by every drawable
		// in the level and is entered concurrently by the render workers, so
		// nothing here may mutate the frame or strip tables.
		//
		// The destination is in world space and the list's current camera maps
		// it; the position overloads are the same draw with the size taken from
		// the source rectangle, which is what the backend used to compute.
		//
		// `origin` IS ADDED TO THE FRAME'S OWN, NOT SUBSTITUTED FOR IT. A sheet
		// may author a pivot per frame (sprite_frame.h) and for years nothing
		// read it; these two overloads are where it now reaches a quad. Both
		// quantities are in unscaled source texels - the same units, so they
		// compose with no conversion, exactly as build_glyph_quad composes a
		// pen with a string's origin (sprite_geometry.h).
		//
		// Addition rather than "the caller's if it gave one" because the second
		// needs a sentinel this signature does not have: a default argument
		// cannot tell a caller that said nothing from one that asked for the
		// top-left corner, so the rule would be a silent policy hanging off
		// whether a Vector2F happened to be zero. A frame with no authored
		// pivot adds zero and every existing caller draws exactly where it did.
		void draw(DrawList& draw_list,
			frame_handle frame,
			const mattmath::RectangleF& destination,
			const Colour& colour = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

		void draw(DrawList& draw_list,
			frame_handle frame,
			const mattmath::Vector2F& position,
			const Colour& colour = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float scale = 1.0f,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

		// The same two, for a caller holding a source rectangle rather than a
		// frame handle - an animation strip's current frame is computed, not
		// named.
		//
		// SO `origin` IS THE CALLER'S ALONE HERE, and the asymmetry with the
		// pair above is not an oversight: there is no frame, so there is no
		// authored pivot to add. A strip's definition has no origin key either,
		// which is why the one caller that reaches these - AnimationObject -
		// loses nothing by it. A sheet that wants a per-frame pivot on an
		// animation is asking for a schema this loader does not have.
		void draw(DrawList& draw_list,
			const mattmath::RectangleI& source,
			const mattmath::RectangleF& destination,
			const Colour& colour = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

		void draw(DrawList& draw_list,
			const mattmath::RectangleI& source,
			const mattmath::Vector2F& position,
			const Colour& colour = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float scale = 1.0f,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

	private:
		NameTable<SpriteFrame> sprite_frames_{ "sprite frame" };
		NameTable<AnimationStrip> animation_strips_{ "animation strip" };
		TextureHandle texture_;
	};
}

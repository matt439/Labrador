#pragma once

#include "engine/core/name_table.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/animation_strip.h"
#include "engine/math/colour.h"
#include <string>

namespace artattack
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
		void draw(DrawList& draw_list,
			frame_handle frame,
			const mattmath::RectangleF& destination,
			const mattmath::Colour& colour = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

		void draw(DrawList& draw_list,
			frame_handle frame,
			const mattmath::Vector2F& position,
			const mattmath::Colour& colour = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			float scale = 1.0f,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

		// The same two, for a caller holding a source rectangle rather than a
		// frame handle - an animation strip's current frame is computed, not
		// named.
		void draw(DrawList& draw_list,
			const mattmath::RectangleI& source,
			const mattmath::RectangleF& destination,
			const mattmath::Colour& colour = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f) const;

		void draw(DrawList& draw_list,
			const mattmath::RectangleI& source,
			const mattmath::Vector2F& position,
			const mattmath::Colour& colour = colour_consts::WHITE,
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

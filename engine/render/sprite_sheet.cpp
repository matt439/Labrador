#include "engine/render/sprite_sheet.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"

#include <string>

using namespace mattmath;

namespace labrador
{
	namespace
	{
		// A position-and-scale draw is a destination-rectangle draw whose size
		// came from the source. The seam takes one destination rectangle, so the
		// arithmetic surfaces here rather than being done five times over inside
		// five backends - DrawList::draw_sprite is implemented once per folder
		// and this is the call it feeds.
		//
		// AND IT IS THE RECTANGLE FORM, SO A SPRITE PLACED THIS WAY SNAPS TO
		// WHOLE PIXELS WHERE TEXT PLACED THE SAME WAY DOES NOT. build_sprite_
		// quad truncates each edge of a destination rectangle; build_glyph_quad
		// deliberately does not go through one, because an advance is
		// fractional in most fonts and a line laid out through rectangles would
		// jitter against itself (sprite_geometry.cpp says so where the two part
		// company). The consequence lives here: a sprite at a fractional
		// position moves in whole pixels while a caption beside it moves in
		// fractions, so the two drift apart by up to a pixel and back. That is
		// a property of the two quad builders rather than of this function, it
		// is what every version of this engine has drawn, and it is stated here
		// because this is the call a client reaches for when it wants a sprite
		// at a position.
		RectangleF destination_from(const RectangleI& source,
			const Vector2F& position, float scale)
		{
			return { position, Vector2F(static_cast<float>(source.width) * scale,
				static_cast<float>(source.height) * scale) };
		}
	}

	SpriteSheet::SpriteSheet(TextureHandle texture,
		NameTable<SpriteFrame> sprite_frames,
		NameTable<AnimationStrip> animation_strips) :
		sprite_frames_(std::move(sprite_frames)),
		animation_strips_(std::move(animation_strips)),
		texture_(texture)
	{

	}

	SpriteSheet::frame_handle SpriteSheet::resolve_sprite_frame(
		const std::string& name) const
	{
		return this->sprite_frames_.resolve(name);
	}

	SpriteSheet::strip_handle SpriteSheet::resolve_animation_strip(
		const std::string& name) const
	{
		return this->animation_strips_.resolve(name);
	}

	const SpriteFrame& SpriteSheet::sprite_frame(frame_handle frame) const
	{
		return this->sprite_frames_.get(frame);
	}

	const AnimationStrip& SpriteSheet::animation_strip(strip_handle strip) const
	{
		return this->animation_strips_.get(strip);
	}

	TextureHandle SpriteSheet::texture() const
	{
		return this->texture_;
	}

	void SpriteSheet::draw(DrawList& draw_list,
		frame_handle frame,
		const RectangleF& destination,
		const Colour& colour,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) const
	{
		const SpriteFrame& sheet_frame = this->sprite_frame(frame);
		this->draw(draw_list, sheet_frame.source_rectangle(), destination,
			colour, rotation, sheet_frame.origin() + origin, flip,
			layer_depth);
	}

	void SpriteSheet::draw(DrawList& draw_list,
		frame_handle frame,
		const Vector2F& position,
		const Colour& colour,
		float rotation,
		const Vector2F& origin,
		float scale,
		SpriteFlip flip,
		float layer_depth) const
	{
		const SpriteFrame& sheet_frame = this->sprite_frame(frame);
		this->draw(draw_list, sheet_frame.source_rectangle(), position, colour,
			rotation, sheet_frame.origin() + origin, scale, flip, layer_depth);
	}

	void SpriteSheet::draw(DrawList& draw_list,
		const RectangleI& source,
		const RectangleF& destination,
		const Colour& colour,
		float rotation,
		const Vector2F& origin,
		SpriteFlip flip,
		float layer_depth) const
	{
		draw_list.draw_sprite(this->texture_, source, destination, colour,
			rotation, origin, flip, layer_depth);
	}

	void SpriteSheet::draw(DrawList& draw_list,
		const RectangleI& source,
		const Vector2F& position,
		const Colour& colour,
		float rotation,
		const Vector2F& origin,
		float scale,
		SpriteFlip flip,
		float layer_depth) const
	{
		draw_list.draw_sprite(this->texture_, source,
			destination_from(source, position, scale), colour, rotation, origin,
			flip, layer_depth);
	}
}

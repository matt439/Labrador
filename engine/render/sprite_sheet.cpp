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
		// came from the source. The old code handed both forms to SpriteBatch,
		// which computed exactly this; the seam takes one destination rectangle,
		// so the arithmetic surfaces here instead of being done twice inside two
		// backends.
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
		this->draw(draw_list, this->sprite_frame(frame).source_rectangle(),
			destination, colour, rotation, origin, flip, layer_depth);
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
		this->draw(draw_list, this->sprite_frame(frame).source_rectangle(),
			position, colour, rotation, origin, scale, flip, layer_depth);
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

#pragma once

#include "engine/core/name_table.h"
#include "engine/render/sprite_frame.h"
#include "engine/render/animation_strip.h"
#include "engine/math/colour.h"
#include "SpriteBatch.h"
#include <string>

namespace artattack
{
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
		SpriteSheet(ID3D11ShaderResourceView* texture,
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

		// Points the sheet at a newly created texture after a device restore. The
		// frame and animation-strip tables are device-independent and survive, so
		// every handle resolved against this sheet stays valid.
		void set_texture(ID3D11ShaderResourceView* texture);

		// Every draw overload is const: a single SpriteSheet is shared by every
		// drawable in the level and is entered concurrently by the render workers,
		// so nothing here may mutate the frame or strip tables.
		void draw(DirectX::SpriteBatch* sprite_batch,
			frame_handle frame,
			const mattmath::Vector2F& position,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin =
				mattmath::Vector2F::ZERO,
			float scale = 1.0f,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f) const;

		void draw(DirectX::SpriteBatch* sprite_batch,
			frame_handle frame,
			const mattmath::RectangleI& destination_rectangle,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin =
				mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f) const;

		void draw(DirectX::SpriteBatch* sprite_batch,
			const RECT* source_rect,
			const mattmath::Vector2F& position,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin =
			mattmath::Vector2F::ZERO,
			float scale = 1.0f,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f) const;

		void draw(DirectX::SpriteBatch* sprite_batch,
			const RECT* source_rect,
			const mattmath::RectangleI& destination_rectangle,
			const mattmath::Colour& color = colour_consts::WHITE,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin =
			mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f) const;
	private:
		NameTable<SpriteFrame> sprite_frames_{ "sprite frame" };
		NameTable<AnimationStrip> animation_strips_{ "animation strip" };
		ID3D11ShaderResourceView* texture_ = nullptr;
	};
}

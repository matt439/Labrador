#pragma once

#include "engine/render/draw_object.h"

namespace artattack
{
	// A string drawn in one font.
	//
	// Text is held wide. DirectXTK's narrow DrawString and MeasureString
	// overloads convert through a utfBuffer owned by the shared SpriteFont -
	// allocated, and possibly reallocated, from inside a const method. Every
	// render worker draws the whole HUD through the same SpriteFont, so the
	// narrow overloads are a data race on the draw path. widen() in
	// text_encoding.h is where narrow content comes across, once.
	//
	// The font name is resolved to a handle at construction. A handle rather than
	// a cached SpriteFont*, because fonts are device resources: a device loss
	// destroys and rebuilds every one of them, and a pointer taken before the loss
	// would be dangling after it. The handle names the registry slot, and the
	// reload refills that same slot.
	class TextObject : public DrawObject
	{
	public:
		TextObject() = default;
		TextObject(const std::wstring& text,
			const std::string& font_name,
			const mattmath::Vector2F& position,
			RenderResources* render_resources,
			const mattmath::Colour& color = colour_consts::WHITE,
			float scale = 1.0f,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			DirectX::SpriteEffects effects = DirectX::SpriteEffects_None,
			float layer_depth = 0.0f);

		virtual void draw(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera) const;
		virtual void draw(DirectX::SpriteBatch* sprite_batch) const;

		// Draws with the given colour, position and scale without storing any of
		// them. TextDropShadow used to draw its shadow by assigning those three
		// members, drawing, and assigning them back - a save/restore that is a
		// data race the moment two render workers run it on the same object.
		void draw_with(DirectX::SpriteBatch* sprite_batch,
			const mattmath::Camera& camera,
			const mattmath::Colour& colour,
			const mattmath::Vector2F& position,
			float scale) const;

		// The box this string occupies, for whoever has to cull or index it.
		//
		// The measurement is cached rather than taken here, because this is on
		// the culling path and MeasureString walks the string: it would be a
		// per-object, per-view, per-frame walk of every label on screen. Only
		// the text and the font change it, so it is retaken in exactly those
		// two setters; position, scale and origin are arithmetic on top.
		//
		// Cached across device loss deliberately. The SpriteFont object is
		// rebuilt, but it is rebuilt from the same font, so the metrics are
		// the same - it is the pointer that goes stale, not the measurement.
		//
		// Ignores draw_rotation(): the box is the unrotated one. Nothing
		// rotates text today, and a rotated string wants its rotated AABB.
		mattmath::RectangleF text_bounds() const;

		// The same box for a hypothetical draw at another position and scale,
		// so TextDropShadow can size its shadow exactly rather than assuming
		// the shadow matches the text.
		mattmath::RectangleF text_bounds_at(const mattmath::Vector2F& position,
			float scale) const;

	protected:
		const std::wstring& text() const;
		RenderResources::FontHandle font() const;
		const mattmath::Vector2F& position() const;
		float scale() const;

		virtual void set_text(const std::wstring& text);
		void set_font(const std::string& font_name);
		virtual void set_position(const mattmath::Vector2F& position);
		virtual void set_scale(float scale);
	private:
		std::wstring text_ = L"";
		RenderResources::FontHandle font_;
		mattmath::Vector2F position_ = mattmath::Vector2F::ZERO;
		float scale_ = 1.0f;

		// Unscaled size of text_ in font_. See text_bounds().
		mattmath::Vector2F measured_size_ = mattmath::Vector2F::ZERO;
		void remeasure();
	};
}

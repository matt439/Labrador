#pragma once

#include "engine/input/direction.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/ui/widget.h"

#include <functional>
#include <string>

namespace components
{
	// A solid rectangle drawn from a one-texel texture: a leaf widget, for the
	// slider's track and its fill.
	//
	// UiTexture would draw the same thing, but it takes a sprite sheet and a
	// frame name, and a white texel in the manifest is all a bar needs. A leaf
	// of your own is the shape PHILOSOPHY (UI) expects a game to write when the
	// engine's set does not fit, and this one is twenty lines.
	class Bar final : public labrador::UiWidget
	{
	public:
		// `block` names a texture whose top-left texel is white.
		Bar(const std::string& name,
			labrador::TextureHandle block,
			const mattmath::RectangleF& rectangle,
			const labrador::Colour& colour);

		void update(float dt) override;
		void draw(labrador::DrawList& draw_list) const override;
		mattmath::RectangleF bounds() const override;
		void set_colour(const labrador::Colour& colour) override;

		void set_rectangle(const mattmath::RectangleF& rectangle);
		const labrador::Colour& colour() const;

	private:
		labrador::TextureHandle block_;
		mattmath::RectangleF rectangle_;
		labrador::Colour colour_;
	};

	// One settings row for a number in a range - a volume, a sensitivity - shown
	// as a label, a bar and the number itself.
	//
	// COPY IT, DO NOT LINK IT. This file and slider.cpp are meant to be
	// dropped into a game and edited there; the README beside them says why
	// components are copied rather than shipped as engine code.
	//
	// A container the cursor lands on, a value the page changes by calling
	// adjust() with left or right, and a callback when it does - the shape of
	// an option row, with an integer in place of a list. Unlike an option row
	// it never wraps - a volume that goes from 100 to 0 on one more press is a
	// trap. The slider does not read input: which keys mean left is the
	// page's binding.
	//
	// Focus paints the label and the fill together, so the bar lights up with
	// the row it belongs to; the track and the number keep their colours.
	class Slider final : public labrador::UiContainer
	{
	public:
		// Called with the new value whenever adjust() moves it, and never when
		// set_value() does.
		using Changed = std::function<void(int value)>;

		// `minimum` must be below `maximum`, `step` above zero and `value`
		// inside the range; anything else throws std::invalid_argument naming
		// the slider (T6). The number is drawn just right of `track`, on the
		// label's line.
		Slider(const std::string& name,
			const std::wstring& label,
			int minimum,
			int maximum,
			int step,
			int value,
			const mattmath::Vector2F& label_position,
			const mattmath::RectangleF& track,
			const std::string& font_name,
			labrador::TextureHandle block,
			labrador::RenderResources* render_resources,
			Changed on_change = nullptr);

		// The children are members and the container holds pointers to them,
		// so a copy or a move would leave the new slider drawing the old one's.
		Slider(const Slider&) = delete;
		Slider& operator=(const Slider&) = delete;

		// Right adds a step and left takes one away, clamped to the range; any
		// other direction is ignored. Returns whether the value changed, so
		// false at either end.
		bool adjust(labrador::Direction direction);

		int value() const;
		int minimum() const;
		int maximum() const;

		// Throws std::invalid_argument for a value outside the range.
		void set_value(int value);

		// The parts, for a page that lines other things up against them. The
		// fill is the lit part of the track: its full height, and as much of
		// its width as the value is of the range.
		const labrador::UiText& label() const;
		const Bar& track() const;
		const Bar& fill() const;
		const labrador::UiText& number() const;

		void set_colour(const labrador::Colour& colour) override;

	private:
		mattmath::RectangleF fill_rectangle() const;
		void show_value();

		int minimum_;
		int maximum_;
		int step_;
		int value_;
		mattmath::RectangleF track_rectangle_;

		// Added in this order, so the fill draws over the track and both
		// under nothing.
		labrador::UiText label_;
		Bar track_;
		Bar fill_;
		labrador::UiText number_;
		Changed on_change_;
	};
}

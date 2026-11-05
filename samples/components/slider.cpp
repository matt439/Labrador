#include "samples/components/slider.h"

#include "engine/math/rectanglei.h"
#include "engine/render/renderer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace labrador;

namespace components
{
	namespace
	{
		// The track is dim, and the number is the colour an option row's
		// value is drawn in, so it never reads as the thing the cursor is
		// on. Focus paints neither. Change both to your game's.
		constexpr Colour track_colour(60, 66, 80);
		constexpr Colour number_colour(235, 190, 90);

		// The gap between the end of the track and the number.
		constexpr float number_gap = 16.0f;

		// The range, after checking it is one. Called from the initialiser
		// list so that no member is built from a range that cannot be drawn.
		int checked_minimum(const std::string& name, int minimum, int maximum,
			int step, int value)
		{
			if (minimum >= maximum || step <= 0 || value < minimum ||
				value > maximum)
			{
				throw std::invalid_argument("Slider '" + name +
					"' needs minimum < maximum, a positive step and a value "
					"inside the range");
			}

			return minimum;
		}

		// The track, after checking a pointer can be placed along it: with
		// no width there is no fraction of the way along, and a drag would
		// quietly do nothing.
		RectangleF checked_track(const std::string& name,
			const RectangleF& track)
		{
			if (!(track.width > 0.0f) || !(track.height >= 0.0f))
			{
				throw std::invalid_argument("Slider '" + name +
					"' needs a track wider than zero, with a height of zero or more");
			}

			return track;
		}
	}

	Bar::Bar(const std::string& name,
		TextureHandle block,
		const RectangleF& rectangle,
		const Colour& colour) :
		UiWidget(name),
		block_(block),
		rectangle_(rectangle),
		colour_(colour)
	{

	}

	void Bar::update(float /*dt*/)
	{

	}

	void Bar::draw(DrawList& draw_list) const
	{
		// widget.h: a leaf that draws checks hidden() itself.
		if (this->hidden())
		{
			return;
		}

		draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
			this->rectangle_, this->colour_, 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);
	}

	RectangleF Bar::bounds() const
	{
		return this->rectangle_;
	}

	void Bar::set_colour(const Colour& colour)
	{
		this->colour_ = colour;
	}

	void Bar::set_rectangle(const RectangleF& rectangle)
	{
		this->rectangle_ = rectangle;
	}

	const Colour& Bar::colour() const
	{
		return this->colour_;
	}

	Slider::Slider(const std::string& name,
		const std::wstring& label,
		int minimum,
		int maximum,
		int step,
		int value,
		const Vector2F& label_position,
		const RectangleF& track,
		const std::string& font_name,
		TextureHandle block,
		RenderResources* render_resources,
		Changed on_change) :
		UiContainer(name),
		minimum_(checked_minimum(name, minimum, maximum, step, value)),
		maximum_(maximum),
		step_(step),
		value_(value),
		track_rectangle_(checked_track(name, track)),
		label_(name + "_label", label, font_name, label_position,
			render_resources),
		track_(name + "_track", block, track, track_colour),
		fill_(name + "_fill", block, track, Colour::white),
		number_(name + "_number", std::to_wstring(value), font_name,
			Vector2F(track.x + track.width + number_gap, label_position.y),
			render_resources, number_colour),
		on_change_(std::move(on_change))
	{
		this->add_child(&this->label_);
		this->add_child(&this->track_);
		this->add_child(&this->fill_);
		this->add_child(&this->number_);
		this->show_value();
	}

	bool Slider::adjust(Direction direction)
	{
		// The step is taken in 64 bits and clamped there, because the range
		// may run to either end of an int and a step past it would overflow.
		long long next = this->value_;

		if (direction == Direction::right)
		{
			next = std::min<long long>(next + this->step_, this->maximum_);
		}
		else if (direction == Direction::left)
		{
			next = std::max<long long>(next - this->step_, this->minimum_);
		}

		return this->change_to(static_cast<int>(next));
	}

	int Slider::value_at(float x) const
	{
		// The constructor refused a track with no width, so this divides by
		// something. In double for the reason fill_rectangle gives.
		const RectangleF& track = this->track_rectangle_;
		const double fraction = std::clamp(
			(static_cast<double>(x) - track.x) / track.width, 0.0, 1.0);
		const double minimum = this->minimum_;
		const double maximum = this->maximum_;
		const double raw = minimum + fraction * (maximum - minimum);
		const double snapped = std::min(maximum,
			minimum + std::round((raw - minimum) / this->step_) * this->step_);

		// A range that is not a whole number of steps still reaches its top.
		const double chosen =
			maximum - raw < std::abs(raw - snapped) ? maximum : snapped;
		return static_cast<int>(chosen);
	}

	bool Slider::drag_to(float x)
	{
		return this->change_to(this->value_at(x));
	}

	int Slider::value() const
	{
		return this->value_;
	}

	int Slider::minimum() const
	{
		return this->minimum_;
	}

	int Slider::maximum() const
	{
		return this->maximum_;
	}

	void Slider::set_value(int value)
	{
		if (value < this->minimum_ || value > this->maximum_)
		{
			throw std::invalid_argument("Slider '" + this->name() +
				"': value out of range");
		}

		this->value_ = value;
		this->show_value();
	}

	const UiText& Slider::label() const
	{
		return this->label_;
	}

	const Bar& Slider::track() const
	{
		return this->track_;
	}

	const Bar& Slider::fill() const
	{
		return this->fill_;
	}

	const UiText& Slider::number() const
	{
		return this->number_;
	}

	void Slider::set_colour(const Colour& colour)
	{
		this->label_.set_colour(colour);
		this->fill_.set_colour(colour);
	}

	RectangleF Slider::fill_rectangle() const
	{
		// In double, so that a range wider than an int can hold is still one.
		const double fraction =
			(static_cast<double>(this->value_) - this->minimum_) /
			(static_cast<double>(this->maximum_) - this->minimum_);

		return RectangleF(this->track_rectangle_.x, this->track_rectangle_.y,
			this->track_rectangle_.width * static_cast<float>(fraction),
			this->track_rectangle_.height);
	}

	bool Slider::change_to(int next)
	{
		if (next == this->value_)
		{
			return false;
		}

		this->value_ = next;
		this->show_value();

		if (this->on_change_)
		{
			this->on_change_(this->value_);
		}

		return true;
	}

	void Slider::show_value()
	{
		this->fill_.set_rectangle(this->fill_rectangle());
		this->number_.set_text(std::to_wstring(this->value_));
	}
}

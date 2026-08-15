#include "engine/input/mouse.h"

namespace labrador
{
	namespace
	{
		// MouseButton::none and MouseButton::count are bounds rather than
		// buttons, and everything below funnels through here so neither can
		// index the mask.
		bool is_button(MouseButton button)
		{
			const unsigned int index = static_cast<unsigned int>(button);
			return index > static_cast<unsigned int>(MouseButton::none) &&
				index < static_cast<unsigned int>(MouseButton::count);
		}

		uint8_t bit_of(MouseButton button)
		{
			return static_cast<uint8_t>(
				1u << static_cast<unsigned int>(button));
		}
	}

	bool MouseState::is_down(MouseButton button) const
	{
		if (!is_button(button))
		{
			return false;
		}
		return (this->buttons & bit_of(button)) != 0;
	}

	bool pressed(const MouseState& now, const MouseState& before,
		MouseButton button)
	{
		if (!now.focused || !before.focused)
		{
			return false;
		}
		return now.is_down(button) && !before.is_down(button);
	}

	bool released(const MouseState& now, const MouseState& before,
		MouseButton button)
	{
		if (!now.focused || !before.focused)
		{
			return false;
		}
		return !now.is_down(button) && before.is_down(button);
	}

	mattmath::Vector2I motion(const MouseState& now, const MouseState& before)
	{
		// The cursor is wherever the desktop left it when focus returns, so
		// the difference across an unfocused frame is not motion - it is the
		// distance between two unrelated places.
		if (!now.focused || !before.focused)
		{
			return mattmath::Vector2I::ZERO;
		}
		return now.position - before.position;
	}

	bool just_focused(const MouseState& now, const MouseState& before)
	{
		return now.focused && !before.focused;
	}

	bool just_unfocused(const MouseState& now, const MouseState& before)
	{
		return !now.focused && before.focused;
	}

	void Mouse::on_move(const mattmath::Vector2I& position)
	{
		this->live_.position = position;
	}

	void Mouse::on_button_down(MouseButton button)
	{
		if (!is_button(button))
		{
			return;
		}
		this->live_.buttons = static_cast<uint8_t>(
			this->live_.buttons | bit_of(button));
	}

	void Mouse::on_button_up(MouseButton button)
	{
		if (!is_button(button))
		{
			return;
		}
		this->live_.buttons = static_cast<uint8_t>(
			this->live_.buttons & ~bit_of(button));
	}

	void Mouse::on_wheel(float notches)
	{
		this->live_.wheel += notches;
	}

	void Mouse::on_wheel_horizontal(float notches)
	{
		this->live_.wheel_horizontal += notches;
	}

	void Mouse::set_focused(bool focused)
	{
		this->live_.focused = focused;

		if (!focused)
		{
			// Buttons go up for the reason keyboard.h documents: the
			// button-up after the cursor leaves is delivered elsewhere, so a
			// bit left set here stays set.
			this->live_.buttons = 0;

			// And the wheel accumulated but not yet polled is dropped. It was
			// turned at this window; delivering it on the frame focus returns
			// would scroll whatever is open then.
			this->live_.wheel = 0.0f;
			this->live_.wheel_horizontal = 0.0f;
		}

		// The position is deliberately left alone. See mouse.h: zero is a real
		// place, and motion() is what keeps the gap honest.
	}

	void Mouse::poll()
	{
		this->previous_ = this->current_;
		this->current_ = this->live_;

		// THE WHEEL RESETS AND THE REST DOES NOT, which is the one asymmetry
		// in this class. Position and buttons are states that persist until
		// something changes them; the wheel is a count of what happened during
		// one frame, so the next frame starts from nothing or every frame
		// reports the sum of every frame before it.
		this->live_.wheel = 0.0f;
		this->live_.wheel_horizontal = 0.0f;
	}

	const MouseState& Mouse::state() const
	{
		return this->current_;
	}

	const MouseState& Mouse::previous_state() const
	{
		return this->previous_;
	}

	bool Mouse::held(MouseButton button) const
	{
		return this->current_.focused && this->current_.is_down(button);
	}

	bool Mouse::pressed(MouseButton button) const
	{
		return labrador::pressed(this->current_, this->previous_, button);
	}

	bool Mouse::released(MouseButton button) const
	{
		return labrador::released(this->current_, this->previous_, button);
	}

	const mattmath::Vector2I& Mouse::position() const
	{
		return this->current_.position;
	}

	mattmath::Vector2I Mouse::motion() const
	{
		return labrador::motion(this->current_, this->previous_);
	}

	float Mouse::wheel() const
	{
		return this->current_.wheel;
	}

	float Mouse::wheel_horizontal() const
	{
		return this->current_.wheel_horizontal;
	}

	bool Mouse::focused() const
	{
		return this->current_.focused;
	}

	bool Mouse::just_focused() const
	{
		return labrador::just_focused(this->current_, this->previous_);
	}

	bool Mouse::just_unfocused() const
	{
		return labrador::just_unfocused(this->current_, this->previous_);
	}
}

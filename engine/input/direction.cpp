#include "engine/input/direction.h"

#include "engine/input/gamepad.h"

#include <cmath>

namespace labrador
{
	Direction stick_direction(const mattmath::Vector2F& stick, float deadzone)
	{
		const mattmath::Vector2F pushed = apply_deadzone(stick, deadzone);

		// Exact equality against ZERO, which is the idiom this engine builds on
		// deliberately: /fp:precise is a compiler setting the whole tree
		// carries (cmake/settings.cmake), and apply_deadzone answers exactly
		// zero inside the deadzone rather than something small. A tolerance
		// here would be a second deadzone applied on top of the first, which
		// is the mistake gamepad.h warns about two lines above the function
		// this calls.
		if (pushed == mattmath::Vector2F::ZERO)
		{
			return Direction::none;
		}

		if (std::abs(pushed.x) >= std::abs(pushed.y))
		{
			return pushed.x < 0.0f ? Direction::left : Direction::right;
		}

		// +y is DOWN here, so a stick pushed away from the player is `up`.
		return pushed.y < 0.0f ? Direction::up : Direction::down;
	}

	Direction pad_direction(const GamepadState& pad, float deadzone)
	{
		// The d-pad first, and it wins. See the header: it is the deliberate
		// input of the two.
		if (pad.is_down(GamepadButton::dpad_up))
		{
			return Direction::up;
		}

		if (pad.is_down(GamepadButton::dpad_down))
		{
			return Direction::down;
		}

		if (pad.is_down(GamepadButton::dpad_left))
		{
			return Direction::left;
		}

		if (pad.is_down(GamepadButton::dpad_right))
		{
			return Direction::right;
		}

		return stick_direction(pad.left_stick, deadzone);
	}

	DirectionRepeat::DirectionRepeat(float first_delay, float repeat_interval) :
		first_delay_(first_delay),
		repeat_interval_(repeat_interval)
	{

	}

	Direction DirectionRepeat::update(Direction held, float dt)
	{
		if (held != this->held_)
		{
			this->held_ = held;
			this->timer_ = this->first_delay_;

			return held;
		}

		if (held == Direction::none)
		{
			return Direction::none;
		}

		this->timer_ -= dt;

		if (this->timer_ > 0.0f)
		{
			return Direction::none;
		}

		// Restarts rather than accumulating. See the header: one move per
		// frame is all the return type can say, so a clock that tried to catch
		// up after a long frame would only make the cursor sprint.
		this->timer_ = this->repeat_interval_;

		return held;
	}

	void DirectionRepeat::reset()
	{
		this->held_ = Direction::none;
		this->timer_ = 0.0f;
	}
}

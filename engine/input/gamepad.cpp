#include "engine/input/gamepad.h"

#include <cmath>

using namespace mattmath;

namespace labrador
{
	bool GamepadState::is_down(GamepadButton button) const
	{
		const uint16_t bit =
			static_cast<uint16_t>(1u << static_cast<unsigned int>(button));
		return (this->buttons & bit) != 0;
	}

	float GamepadState::trigger(GamepadTrigger trigger) const
	{
		return trigger == GamepadTrigger::left ?
			this->left_trigger : this->right_trigger;
	}

	bool pressed(const GamepadState& now, const GamepadState& before,
		GamepadButton button)
	{
		// The slot on both frames first, then the edge across them. Without the
		// first half the frame a pad appears on is a press for every button it
		// arrives holding - see gamepad.h for why that is a statement about
		// occupancy and not about identity.
		return now.connected && before.connected &&
			now.is_down(button) && !before.is_down(button);
	}

	bool released(const GamepadState& now, const GamepadState& before,
		GamepadButton button)
	{
		// Symmetrically: a pad that vanishes with a button down did not release
		// it, and a client that wants to hear about that asks for the
		// disconnect. gamepad.h spells the idiom.
		return now.connected && before.connected &&
			!now.is_down(button) && before.is_down(button);
	}

	bool trigger_held(const GamepadState& now, GamepadTrigger trigger,
		float threshold)
	{
		return now.trigger(trigger) > threshold;
	}

	bool trigger_pressed(const GamepadState& now, const GamepadState& before,
		GamepadTrigger trigger, float threshold)
	{
		return now.connected && before.connected &&
			now.trigger(trigger) > threshold &&
			before.trigger(trigger) <= threshold;
	}

	bool trigger_released(const GamepadState& now, const GamepadState& before,
		GamepadTrigger trigger, float threshold)
	{
		return now.connected && before.connected &&
			now.trigger(trigger) <= threshold &&
			before.trigger(trigger) > threshold;
	}

	bool just_connected(const GamepadState& now, const GamepadState& before)
	{
		return now.connected && !before.connected;
	}

	bool just_disconnected(const GamepadState& now, const GamepadState& before)
	{
		return !now.connected && before.connected;
	}

	float apply_deadzone(float value, float deadzone)
	{
		// A deadzone at or past full deflection would leave no live range to
		// rescale into, and a negative one has nothing to cut, so both mean
		// "pass it through" rather than divide by zero.
		if (deadzone <= 0.0f || deadzone >= 1.0f)
		{
			return value;
		}

		const float magnitude = std::fabs(value);
		if (magnitude <= deadzone)
		{
			return 0.0f;
		}

		const float scaled = (magnitude - deadzone) / (1.0f - deadzone);
		return value < 0.0f ? -scaled : scaled;
	}

	Vector2F apply_deadzone(const Vector2F& stick, float deadzone)
	{
		if (deadzone <= 0.0f || deadzone >= 1.0f)
		{
			return stick;
		}

		const float magnitude = stick.length();
		if (magnitude <= deadzone)
		{
			return Vector2F::ZERO;
		}

		// The direction survives untouched and only the magnitude is rescaled,
		// which is what makes this different from doing the scalar version to
		// each axis.
		const float scaled = (magnitude - deadzone) / (1.0f - deadzone);
		return stick * (scaled / magnitude);
	}
}

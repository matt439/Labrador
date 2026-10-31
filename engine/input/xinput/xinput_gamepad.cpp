#include "engine/input/xinput/xinput_gamepad.h"
#include "engine/math/vector2f.h"

#include <cstdint>

namespace labrador
{
	namespace
	{
		uint16_t bit(GamepadButton button)
		{
			return static_cast<uint16_t>(
				1u << static_cast<unsigned int>(button));
		}

		uint16_t buttons_of(const DirectX::GamePad::State& pad)
		{
			uint16_t buttons = 0;
			const auto set = [&buttons](bool down, GamepadButton button)
			{
				if (down)
				{
					buttons = static_cast<uint16_t>(buttons | bit(button));
				}
			};

			set(pad.buttons.a, GamepadButton::a);
			set(pad.buttons.b, GamepadButton::b);
			set(pad.buttons.x, GamepadButton::x);
			set(pad.buttons.y, GamepadButton::y);
			set(pad.buttons.leftShoulder, GamepadButton::left_shoulder);
			set(pad.buttons.rightShoulder, GamepadButton::right_shoulder);
			set(pad.buttons.leftStick, GamepadButton::left_stick);
			set(pad.buttons.rightStick, GamepadButton::right_stick);
			set(pad.buttons.back, GamepadButton::back);
			set(pad.buttons.start, GamepadButton::start);
			set(pad.dpad.up, GamepadButton::dpad_up);
			set(pad.dpad.down, GamepadButton::dpad_down);
			set(pad.dpad.left, GamepadButton::dpad_left);
			set(pad.dpad.right, GamepadButton::dpad_right);

			return buttons;
		}
	}

	GamepadState gamepad_state_from_xinput(const DirectX::GamePad::State& pad)
	{
		GamepadState result;
		if (!pad.IsConnected())
		{
			return result;
		}

		result.connected = true;
		result.left_stick =
			Vector2F(pad.thumbSticks.leftX, -pad.thumbSticks.leftY);
		result.right_stick =
			Vector2F(pad.thumbSticks.rightX, -pad.thumbSticks.rightY);
		result.left_trigger = pad.triggers.left;
		result.right_trigger = pad.triggers.right;
		result.buttons = buttons_of(pad);

		return result;
	}
}

#include "engine/input/web/standard_gamepad.h"
#include "engine/math/vector2f.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace labrador
{
	namespace
	{
		// The W3C "standard" layout, by index. Buttons 6 and 7 are the
		// triggers, read as analogue values below; 16 is the guide button,
		// which this engine has no name for and which a standard pad may not
		// have, so it is not asked for.
		struct ButtonIndex
		{
			int index;
			GamepadButton button;
		};

		constexpr ButtonIndex standard_buttons[] = {
			{ 0, GamepadButton::a },
			{ 1, GamepadButton::b },
			{ 2, GamepadButton::x },
			{ 3, GamepadButton::y },
			{ 4, GamepadButton::left_shoulder },
			{ 5, GamepadButton::right_shoulder },
			{ 8, GamepadButton::back },
			{ 9, GamepadButton::start },
			{ 10, GamepadButton::left_stick },
			{ 11, GamepadButton::right_stick },
			{ 12, GamepadButton::dpad_up },
			{ 13, GamepadButton::dpad_down },
			{ 14, GamepadButton::dpad_left },
			{ 15, GamepadButton::dpad_right },
		};

		constexpr int standard_button_count = 16;
		constexpr int standard_axis_count = 4;
		constexpr int left_trigger_index = 6;
		constexpr int right_trigger_index = 7;

		bool holds(const GamepadSlots& slots, int index)
		{
			return std::find(slots.begin(), slots.end(), index) != slots.end();
		}
	}

	bool is_standard_gamepad(const EmscriptenGamepadEvent& pad)
	{
		return std::strcmp(pad.mapping, "standard") == 0 &&
			pad.numButtons >= standard_button_count &&
			pad.numAxes >= standard_axis_count;
	}

	GamepadState gamepad_state_from_standard(const EmscriptenGamepadEvent& pad)
	{
		GamepadState result;
		if (!pad.connected || !is_standard_gamepad(pad))
		{
			return result;
		}

		result.connected = true;
		result.left_stick = Vector2F(static_cast<float>(pad.axis[0]),
			static_cast<float>(pad.axis[1]));
		result.right_stick = Vector2F(static_cast<float>(pad.axis[2]),
			static_cast<float>(pad.axis[3]));

		result.left_trigger =
			static_cast<float>(pad.analogButton[left_trigger_index]);
		result.right_trigger =
			static_cast<float>(pad.analogButton[right_trigger_index]);

		uint16_t buttons = 0;
		for (const ButtonIndex& entry : standard_buttons)
		{
			if (pad.digitalButton[entry.index])
			{
				buttons = static_cast<uint16_t>(buttons |
					(1u << static_cast<unsigned int>(entry.button)));
			}
		}
		result.buttons = buttons;

		return result;
	}

	void assign_gamepad_slots(GamepadSlots& slots, std::span<const int> present)
	{
		for (int& slot : slots)
		{
			if (slot != no_gamepad &&
				std::find(present.begin(), present.end(), slot) == present.end())
			{
				slot = no_gamepad;
			}
		}

		for (const int index : present)
		{
			if (holds(slots, index))
			{
				continue;
			}
			const GamepadSlots::iterator free_slot =
				std::find(slots.begin(), slots.end(), no_gamepad);
			if (free_slot == slots.end())
			{
				return;
			}
			*free_slot = index;
		}
	}
}

#include "engine/input/gamepad_reader.h"

#include <GamePad.h>
#include <memory>

using namespace mattmath;

namespace artattack
{
	struct GamepadReader::Impl
	{
		DirectX::GamePad gamepad;
	};

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

	GamepadReader::GamepadReader() :
		impl_(std::make_unique<Impl>())
	{
	}

	GamepadReader::~GamepadReader() = default;
	GamepadReader::GamepadReader(GamepadReader&&) noexcept = default;
	GamepadReader& GamepadReader::operator=(GamepadReader&&) noexcept = default;

	GamepadState GamepadReader::read(int slot) const
	{
		GamepadState result;

		// DEAD_ZONE_NONE, deliberately. The deadzone is applied once, by
		// whoever knows what the stick is being used for, with apply_deadzone.
		// Asking for one here and letting a caller apply a second by hand is
		// exactly the defect this module was written to remove - and the two
		// copies it replaces asked for *different* ones, so the same physical
		// stick position meant two things depending on which was reading.
		const DirectX::GamePad::State pad = this->impl_->gamepad.GetState(
			slot, DirectX::GamePad::DEAD_ZONE_NONE);
		if (!pad.IsConnected())
		{
			return result;
		}

		result.connected = true;

		// The one negation, in the one place. Pad APIs report +y up; this
		// engine is +y down everywhere else.
		result.left_stick =
			Vector2F(pad.thumbSticks.leftX, -pad.thumbSticks.leftY);
		result.right_stick =
			Vector2F(pad.thumbSticks.rightX, -pad.thumbSticks.rightY);

		result.left_trigger = pad.triggers.left;
		result.right_trigger = pad.triggers.right;
		result.buttons = buttons_of(pad);

		return result;
	}

	void GamepadReader::suspend()
	{
		this->impl_->gamepad.Suspend();
	}

	void GamepadReader::resume()
	{
		this->impl_->gamepad.Resume();
	}
}

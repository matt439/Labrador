#include "engine/input/gamepads.h"

#include <stdexcept>
#include <string>

namespace labrador
{
	Gamepads::Gamepads(GamepadReader* reader) :
		reader_(reader)
	{
	}

	void Gamepads::poll()
	{
		for (int slot = 0; slot < max_count; ++slot)
		{
			this->previous_[slot] = this->current_[slot];
			this->current_[slot] = this->reader_->read(slot);
		}
	}

	const GamepadState& Gamepads::checked(const GamepadState* states,
		int slot) const
	{
		if (slot < 0 || slot >= max_count)
		{
			throw std::out_of_range("Gamepads: slot " + std::to_string(slot) +
				" is not one of the " + std::to_string(max_count) +
				" the engine polls");
		}
		return states[slot];
	}

	const GamepadState& Gamepads::state(int slot) const
	{
		return this->checked(this->current_, slot);
	}

	const GamepadState& Gamepads::previous_state(int slot) const
	{
		return this->checked(this->previous_, slot);
	}

	// Everything below is a forwarder to the free functions in gamepad.h,
	// which is where the edge logic is so that a test can reach it without a
	// controller being plugged into the machine.

	bool Gamepads::held(int slot, GamepadButton button) const
	{
		return this->state(slot).is_down(button);
	}

	bool Gamepads::pressed(int slot, GamepadButton button) const
	{
		return labrador::pressed(this->state(slot),
			this->previous_state(slot), button);
	}

	bool Gamepads::released(int slot, GamepadButton button) const
	{
		return labrador::released(this->state(slot),
			this->previous_state(slot), button);
	}

	bool Gamepads::trigger_held(int slot, GamepadTrigger trigger,
		float threshold) const
	{
		return labrador::trigger_held(this->state(slot), trigger, threshold);
	}

	bool Gamepads::trigger_pressed(int slot, GamepadTrigger trigger,
		float threshold) const
	{
		return labrador::trigger_pressed(this->state(slot),
			this->previous_state(slot), trigger, threshold);
	}

	bool Gamepads::trigger_released(int slot, GamepadTrigger trigger,
		float threshold) const
	{
		return labrador::trigger_released(this->state(slot),
			this->previous_state(slot), trigger, threshold);
	}

	bool Gamepads::connected(int slot) const
	{
		return this->state(slot).connected;
	}

	bool Gamepads::just_connected(int slot) const
	{
		return labrador::just_connected(this->state(slot),
			this->previous_state(slot));
	}

	bool Gamepads::just_disconnected(int slot) const
	{
		return labrador::just_disconnected(this->state(slot),
			this->previous_state(slot));
	}
}

#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/xinput/xinput_gamepad.h"

#include <GamePad.h>
#include <memory>

using namespace labrador;

namespace labrador
{
	struct GamepadReader::Impl
	{
		DirectX::GamePad gamepad;

		// What the last sample() read, by XInput user index, which is already
		// a slot that holds while its pad stays connected.
		GamepadState states[Gamepads::max_count];
	};

	namespace
	{
		GamepadState read_pad(DirectX::GamePad& gamepad, int slot)
		{
			return gamepad_state_from_xinput(
				gamepad.GetState(slot, xinput_dead_zone));
		}
	}

	GamepadReader::GamepadReader() :
		impl_(std::make_unique<Impl>())
	{
	}

	GamepadReader::~GamepadReader() = default;
	GamepadReader::GamepadReader(GamepadReader&&) noexcept = default;
	GamepadReader& GamepadReader::operator=(GamepadReader&&) noexcept = default;

	void GamepadReader::sample()
	{
		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			this->impl_->states[slot] = read_pad(this->impl_->gamepad, slot);
		}
	}

	GamepadState GamepadReader::read(int slot) const
	{
		if (slot < 0 || slot >= Gamepads::max_count)
		{
			return GamepadState();
		}
		return this->impl_->states[slot];
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

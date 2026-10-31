#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/vibration_output.h"
#include "engine/input/xinput/xinput_gamepad.h"

#include <GamePad.h>
#include <memory>

namespace labrador
{
	namespace
	{
		// XInput's left motor is the heavy, low-frequency one. A motor holds
		// whatever level it was last given, so VibrationOutput's stop is what
		// ends a request here. DirectXTK's SetVibration answers false for an
		// empty slot, and an empty slot asked to stop is harmless.
		struct XInputVibration
		{
			DirectX::GamePad* gamepad = nullptr;

			bool send(int slot, const Vibration& vibration)
			{
				return this->gamepad->SetVibration(slot,
					vibration.low_frequency, vibration.high_frequency);
			}

			void stop(int slot)
			{
				static_cast<void>(
					this->gamepad->SetVibration(slot, 0.0f, 0.0f));
			}
		};
	}

	struct GamepadReader::Impl
	{
		DirectX::GamePad gamepad;

		// What the last sample() read, by XInput user index, which is already
		// a slot that holds while its pad stays connected.
		GamepadState states[Gamepads::max_count];

		// After the pad object, so it is destroyed first and its last stop
		// still has a pad API to reach.
		VibrationOutput<XInputVibration> vibration{
			XInputVibration{ &this->gamepad } };
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
		const double now = vibration_clock_seconds();
		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			this->impl_->states[slot] = read_pad(this->impl_->gamepad, slot);

			// XInput keeps a user index for a pad while it stays connected,
			// so a connected slot is the pad it was.
			this->impl_->vibration.sampled(slot,
				this->impl_->states[slot].connected, now);
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
		// Motors first. DirectXTK's XInput Suspend sends nothing on Windows
		// 10 and later, so a motor left running would hold its level through
		// the background.
		this->impl_->vibration.suspend();
		this->impl_->gamepad.Suspend();
	}

	void GamepadReader::resume()
	{
		this->impl_->gamepad.Resume();
		this->impl_->vibration.resume();
	}

	bool GamepadReader::vibrate(int slot, const Vibration& vibration)
	{
		return this->impl_->vibration.start(slot, vibration,
			vibration_clock_seconds());
	}

	void GamepadReader::stop_vibration(int slot)
	{
		this->impl_->vibration.stop(slot);
	}
}

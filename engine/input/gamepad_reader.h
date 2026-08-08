#pragma once

#include "engine/input/gamepad.h"

#include <memory>

namespace artattack
{
	// The input backend seam, and it is one function wide.
	//
	// A concrete class with one implementation chosen at build time, for the
	// reasons renderer.h gives at length and will not repeat here: asking for a
	// backend that was not built is a missing symbol rather than a run-time
	// answer (T5), a vtable on the frame path is a tax nothing has asked to pay
	// (T8), and promoting a concrete class to an interface later is mechanical
	// and changes no call site. The DirectXTK/XInput implementation is
	// engine/input/xinput/, and nothing outside that folder names a pad API -
	// which is the whole difference from what this replaces, where the real
	// input API was a raw DirectX::GamePad* on Application.
	class GamepadReader
	{
	public:
		GamepadReader();
		~GamepadReader();

		GamepadReader(GamepadReader&&) noexcept;
		GamepadReader& operator=(GamepadReader&&) noexcept;

		// One slot, as it is right now. Slots are 0 .. Gamepads::max_count - 1;
		// anything else answers a disconnected state rather than throwing,
		// because "there is no pad there" is the same answer either way.
		GamepadState read(int slot) const;

		// The window lost or regained the foreground. A suspended reader
		// answers disconnected for every slot, so a button held down while the
		// game is in the background is not waiting to be delivered as input the
		// moment it comes back.
		void suspend();
		void resume();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}

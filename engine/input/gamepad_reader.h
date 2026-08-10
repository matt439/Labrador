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
		//
		// THAT LAST CLAUSE IS NOT THIS CLASS'S TO KEEP, and it should not be
		// read as the thing that makes it true. Both calls forward straight to
		// the backend with no flag of their own, the backend implements
		// suspension differently per pad API, and which pad API this build
		// compiles is not chosen here: the vcpkg DirectXTK target carries
		// USING_XINPUT in its interface definitions, which pre-empts the
		// selection inside GamePad.h before it runs. Whether XInput honours a
		// suspend on a current Windows is the backend's affair and is not
		// verified in this tree.
		//
		// What is guaranteed is narrower and lives in gamepad.h: an edge
		// requires the slot to have been occupied on both frames, so every
		// absence this reader DOES report - a replug, the first poll, a
		// suspension that works - is suppressed rather than delivered as a
		// press. That holds whichever backend is behind this seam.
		void suspend();
		void resume();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}

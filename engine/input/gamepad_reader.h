#pragma once

#include "engine/input/gamepad.h"

#include <memory>

namespace labrador
{
	// The input backend seam: a snapshot, and the slots read out of it.
	//
	// A concrete class with one implementation chosen at build time, for the
	// reasons engine/render/SEAM.md#2 gives for the renderer: asking for a
	// backend that was not built is a missing symbol rather than a run-time
	// answer (T5), a vtable on the frame path is a tax nothing has asked to
	// pay (T8), and promoting a concrete class to an interface later is
	// mechanical and changes no call site. The DirectXTK/XInput implementation
	// is engine/input/xinput/, and nothing outside that folder names a pad API.
	class GamepadReader
	{
	public:
		GamepadReader();
		~GamepadReader();

		GamepadReader(GamepadReader&&) noexcept;
		GamepadReader& operator=(GamepadReader&&) noexcept;

		// Takes the snapshot every read() answers from until the next call.
		// Gamepads::poll calls it once a frame, before reading any slot, so
		// the four slots of one frame are one moment of the pad API rather
		// than four, and a pad API that has to be asked for everything at once
		// - the browser's - is asked once.
		void sample();

		// One slot, as the last sample() saw it, and disconnected before the
		// first. Slots are 0 .. Gamepads::max_count - 1; anything else answers
		// a disconnected state rather than throwing, because "there is no pad
		// there" is the same answer either way.
		//
		// Which pad is in which slot is the backend's to decide, and every
		// backend keeps one rule: a pad holds its slot for as long as it stays
		// connected. XInput's user index already is that; a browser numbers
		// pads by its own count, which can pass the four slots there are, so
		// engine/input/web/ hands each new pad the lowest free slot instead.
		GamepadState read(int slot) const;

		// The game went into the background or came back: on Windows the
		// window lost the foreground or was minimised, and in a browser the
		// page was hidden. Not the keyboard leaving a canvas, which a player
		// on a pad never asked for (Application says how the two are told
		// apart). A suspended reader answers disconnected for every slot, so a
		// button held down while the game is in the background is not waiting
		// to be delivered as input the moment it comes back.
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
		//
		// Suspension also stops every pad's motors and refuses vibration
		// until resume, and resume replays nothing: a rumble asked for before
		// the game went into the background does not start again when it
		// comes back.
		void suspend();
		void resume();

		// The output half: one bounded request to `slot`'s two motors, which
		// Gamepads::vibrate has already checked, and an explicit stop. vibrate
		// answers whether the pad API took the request. False is unavailable
		// output - a slot out of range or with no pad, a pad whose rumble the
		// API cannot reach, or a suspended reader - and nothing is kept to be
		// sent later. A request ends when its seconds pass, at a stop, a newer
		// request on its slot, or when its pad leaves; sample() is where its
		// time and its pad are checked, so its end is seen within a frame.
		// Destroying the reader stops every pad it vibrated, without throwing.
		//
		// What reaches a motor is the pad API's affair and is not verified by
		// any test in this tree. engine/input/vibration_output.h holds the
		// lifetime rules both backends share, and InputTests holds those.
		bool vibrate(int slot, const Vibration& vibration);
		void stop_vibration(int slot);

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}

#pragma once

#include "engine/input/gamepad.h"
#include "engine/input/gamepad_reader.h"

namespace labrador
{
	// Every pad, this frame and last, and the edges between them.
	//
	// A game reads the borrowed instance from Application::gamepads(). The
	// shell constructs it and calls poll(); game code does not advance the
	// snapshots itself.
	//
	// WHY THE ENGINE POLLS THIS AND A GAME DOES NOT. An edge is "down now, up
	// last frame", so it is only right if "last frame" is the frame before this
	// one. An edge detector a game keeps for itself advances its "previous"
	// only on the frames its owner happens to be running, so it needs priming
	// by hand at every transition, and a second one - menus beside gameplay -
	// drifts from the first. Application polls this once per frame, before
	// any state updates, so every reader measures across the same two frames
	// and nothing is primed.
	//
	// A slot is a slot. The backend assigns it (GamepadReader::read) and a pad
	// keeps it while it stays connected, but it is not stable across a replug
	// - a controller that comes back in a different slot is a different slot. Binding a player
	// to one for the length of a match is a decision a game makes; the engine
	// reports connected(), just_connected() and just_disconnected() so that it
	// can make it, and makes none itself.
	class Gamepads
	{
	public:
		static constexpr int max_count = 4;

		// For the shell or a device fixture. A game uses Application::gamepads().
		// `reader` is borrowed and must outlive this.
		explicit Gamepads(GamepadReader* reader);

		// Called by the shell before state updates. Samples the reader once
		// and reads every slot. What was current becomes previous, so every
		// edge below is measured across exactly one frame.
		void poll();

		// Throw std::out_of_range naming the slot if it is not in
		// [0, max_count). A slot outside that is a bug in the caller rather
		// than an absent pad, and the two deserve different answers (T6).
		const GamepadState& state(int slot) const;
		const GamepadState& previous_state(int slot) const;

		bool held(int slot, GamepadButton button) const;
		bool pressed(int slot, GamepadButton button) const;
		bool released(int slot, GamepadButton button) const;

		// A trigger is analogue, so an edge on one needs the threshold that
		// makes it a button. The caller supplies it because no single value is
		// right for every use of the same trigger.
		bool trigger_held(int slot, GamepadTrigger trigger,
			float threshold) const;
		bool trigger_pressed(int slot, GamepadTrigger trigger,
			float threshold) const;
		bool trigger_released(int slot, GamepadTrigger trigger,
			float threshold) const;

		bool connected(int slot) const;
		bool just_connected(int slot) const;
		bool just_disconnected(int slot) const;

		// Sends one bounded request to the pad in `slot`, replacing any request
		// still running there; gamepad.h's Vibration says what the fields
		// mean and what is the game's to decide. The slot is checked as
		// state() checks it, then the request as check_vibration checks it,
		// and both throw rather than send something else.
		//
		// unavailable when the slot had no pad at the last poll or the reader
		// could not pass the request on; nothing is then waiting to be sent.
		// submitted when the pad API took it, which is not evidence that a
		// motor turned. A request is stopped when its seconds pass, its pad
		// leaves the slot, the reader is suspended or the reader is destroyed,
		// and resuming does not replay it. Which of a game's own transitions
		// should also stop one - a pause menu, a scene change - is the game's
		// call, with stop_vibration.
		VibrationResult vibrate(int slot, const Vibration& vibration);

		// Stops `slot`'s motors and forgets its request. Safe on an empty slot
		// and on one with nothing running. Throws std::out_of_range for a slot
		// state() would refuse.
		void stop_vibration(int slot);

	private:
		const GamepadState& checked(const GamepadState* states,
			int slot) const;

		GamepadReader* reader_ = nullptr;
		GamepadState current_[max_count];
		GamepadState previous_[max_count];
	};
}

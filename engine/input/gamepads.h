#pragma once

#include "engine/input/gamepad.h"
#include "engine/input/gamepad_reader.h"

namespace artattack
{
	// Every pad, this frame and last, and the edges between them.
	//
	// WHY THE ENGINE POLLS THIS AND A GAME DOES NOT. An edge is "down now, up
	// last frame", so it is only right if "last frame" is the frame before this
	// one. The paint-shooter had two edge detectors, one for gameplay and one
	// for menus, each advancing its own "previous" only on the frames its owner
	// happened to be running. Both therefore needed priming by hand at every
	// transition; both carried a prime() with a paragraph of comment describing
	// a bug it had shipped; and the two copies had already drifted apart - one
	// asked the pad API for a circular deadzone and the other for none, so the
	// same stick position meant two things. Application polls this once per
	// frame, before any state updates, and the whole shape goes with it.
	//
	// A slot is a slot. It is the index the pad API answers for, not the order
	// pads were plugged in, and it is not stable across a replug - a controller
	// that comes back in a different slot is a different slot. Binding a player
	// to one for the length of a match is a decision a game makes; the engine
	// reports connected(), just_connected() and just_disconnected() so that it
	// can make it, and makes none itself.
	class Gamepads
	{
	public:
		static constexpr int max_count = 4;

		// `reader` is borrowed and must outlive this.
		explicit Gamepads(GamepadReader* reader);

		// Reads every slot. What was current becomes previous, so every edge
		// below is measured across exactly one frame.
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

	private:
		const GamepadState& checked(const GamepadState* states,
			int slot) const;

		GamepadReader* reader_ = nullptr;
		GamepadState current_[max_count];
		GamepadState previous_[max_count];
	};
}

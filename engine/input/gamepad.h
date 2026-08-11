#pragma once

#include "engine/math/vector2f.h"

#include <cstdint>

namespace artattack
{
	// WHAT IS DELIBERATELY ABSENT: an action map. ARCHITECTURE gives this
	// module "devices and action mapping", and this half is the devices. A
	// table from a named action to a button, filled from data, is the other
	// half - and neither client has asked for one: the paint-shooter and the
	// sample both spell their bindings in code, and neither has a rebinding
	// screen to fill such a table from. Building it now would be a speculative
	// framework (T1). What is here is what both clients actually use, and an
	// action map is a layer above it rather than a change to it.

	// Every button a pad has, one bit each in GamepadState::buttons.
	enum class GamepadButton : uint16_t
	{
		a,
		b,
		x,
		y,
		left_shoulder,
		right_shoulder,
		left_stick,
		right_stick,
		back,
		start,
		dpad_up,
		dpad_down,
		dpad_left,
		dpad_right,
	};

	// The two analogue triggers. Named rather than folded into GamepadButton
	// because an edge on one is only meaningful against a threshold, and the
	// threshold is the caller's: a jump trigger and a shoot trigger do not
	// agree on one.
	enum class GamepadTrigger
	{
		left,
		right,
	};

	// One pad, one frame. A value, because an edge detector needs two of them
	// and a test needs to be able to write one by hand.
	struct GamepadState
	{
		// False means the slot is empty, and every field below is then neutral
		// rather than whatever was last read out of it.
		bool connected = false;

		// Screen convention: +x right, +y DOWN - the same convention every
		// rectangle, viewport and camera in this engine already uses. Pad APIs
		// report +y up, and converting it here once is why nothing above this
		// line has to remember to negate it.
		//
		// Raw: no deadzone has been applied. Which deadzone is the caller's
		// decision, made once with apply_deadzone below - a menu comparing
		// against a presence threshold and a player walking at an analogue
		// speed want different ones, and asking the pad API for one and then
		// applying a second by hand is how a stick ends up with half its travel
		// dead and the other half starting at 0.5.
		mattmath::Vector2F left_stick = mattmath::Vector2F::ZERO;
		mattmath::Vector2F right_stick = mattmath::Vector2F::ZERO;

		float left_trigger = 0.0f;
		float right_trigger = 0.0f;

		uint16_t buttons = 0;

		bool is_down(GamepadButton button) const;
		float trigger(GamepadTrigger trigger) const;
	};

	// The edges between two frames of one pad.
	//
	// Free functions over two values, and Gamepads' methods of the same names
	// are forwarders to them. That is what makes the whole of the edge logic
	// testable without a device: a test writes the two frames by hand rather
	// than needing a controller to be plugged into the machine running it.
	//
	// AN EDGE REQUIRES THE SLOT TO HAVE BEEN OCCUPIED ON BOTH FRAMES. All four
	// of the edge functions below - pressed, released, trigger_pressed,
	// trigger_released - answer false unless `now.connected` and
	// `before.connected` are both true, and that conjunction is part of the
	// contract rather than an implementation detail.
	//
	// It did not used to be, and the absence was reachable three ways. The
	// process's first poll compares against a default-constructed previous, so
	// a pad already holding A when the game starts pressed it. A replug reads
	// as absent and then present again, so it pressed everything it came back
	// holding. And a reader that reports absence for any other reason - see
	// GamepadReader::suspend - hands the same phantom edge to whatever
	// resumes. None of it was a client's to guard: the obvious
	// `if (pads.connected(slot))` is TRUE on the offending frame, which is
	// precisely the frame the slot became occupied, so the guard a careful
	// caller writes does not work and the correct one - gating every press site
	// on !just_connected(slot) - has to be remembered at every site.
	//
	// IT SAYS OCCUPIED, NOT "THE SAME DEVICE". connected && connected does not
	// establish identity and cannot: gamepads.h states that this engine
	// deliberately does not track it, and a backend is free to drop one pad and
	// add another inside a single scan, so a controller leaving a slot and a
	// different one landing in it between two polls is a cross-device edge with
	// no absent frame in between for this rule to catch. That case belongs to
	// the caller, through just_connected/just_disconnected, and claiming
	// otherwise here would be the same kind of promise this rule exists to stop
	// making.
	//
	// THE DISCONNECT SIDE LOSES NOTHING, but it does change spelling. A pad
	// that vanishes with a button down no longer produces released() on that
	// frame - held() is false there too - so a client keying "stop firing" off
	// a release asks for the disconnect instead:
	//
	//     just_disconnected(slot) && previous_state(slot).is_down(button)
	//
	// One consequence for a hand-written state: a test double or a replay
	// source that fills a GamepadState and forgets `connected = true` now gets
	// silence from every edge rather than working by accident. That is the
	// useful direction to fail in, and it is the reason the field is documented
	// as neutral-not-stale above.
	bool pressed(const GamepadState& now, const GamepadState& before,
		GamepadButton button);
	bool released(const GamepadState& now, const GamepadState& before,
		GamepadButton button);

	// A trigger is analogue, so an edge on one is only meaningful against the
	// threshold that makes it a button, and the caller owns that.
	bool trigger_held(const GamepadState& now, GamepadTrigger trigger,
		float threshold);
	bool trigger_pressed(const GamepadState& now, const GamepadState& before,
		GamepadTrigger trigger, float threshold);
	bool trigger_released(const GamepadState& now, const GamepadState& before,
		GamepadTrigger trigger, float threshold);

	bool just_connected(const GamepadState& now, const GamepadState& before);
	bool just_disconnected(const GamepadState& now, const GamepadState& before);

	// Rescales `value` so the first live reading is 0 and full deflection is
	// still 1, and answers 0 inside the deadzone.
	//
	// The rescale is the point. Zeroing below the deadzone and passing the raw
	// value above it makes the stick jump from nothing straight to the
	// threshold, so a player who wants to creep cannot, and a deadzone of 0.5
	// costs half the stick's travel and then starts at half speed.
	float apply_deadzone(float value, float deadzone);

	// The same, radially: the magnitude is rescaled and the direction is left
	// alone. A per-axis deadzone on a stick is a square hole in a round gate -
	// it lets a diagonal through that a straight push of the same distance
	// would not.
	mattmath::Vector2F apply_deadzone(const mattmath::Vector2F& stick,
		float deadzone);
}

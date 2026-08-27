#pragma once

#include "engine/math/vector2f.h"

namespace labrador
{
	struct GamepadState;

	// Which way the player pushed.
	//
	// THIS TYPE USED TO LIVE IN engine/ui/navigation.h, above a sentence that
	// said it was "produced by the input module from a stick or a d-pad;
	// consumed here as a pure direction with no device in it". The second half
	// was true. The first was not: `grep -rn Direction engine/ | grep -v
	// engine/ui/` was empty, and no producer existed anywhere in the tree. A
	// header stating a false fact about another module is a defect on its own,
	// which is what docs/survey/2026-08-26.md section 3.3 called it.
	//
	// It moved here rather than the producer moving there, because the
	// sentence had the dependency the right way round and ARCHITECTURE's module
	// table already allows it: `ui` may depend on `input`, and `input` may
	// depend on core and math alone. A direction is what a device reports;
	// navigating widgets with one is what a menu does with it. Putting the
	// producer in `ui` would have made the input module depend on the widget
	// set to describe a stick, which is the table read backwards.
	//
	// That edge of the table had never been exercised - nothing in engine/ui/
	// included anything from engine/input/ - so this is the first thing that
	// stands on it.
	enum class Direction
	{
		none,
		up,
		down,
		left,
		right,
	};

	// Which way a stick is pushed, or none inside `deadzone`.
	//
	// The quadrant test, which is the second of the three things every client
	// wiring a menu writes by hand. The first is the deadzone and it is
	// apply_deadzone in gamepad.h, which this calls rather than reimplements -
	// radially, because a per-axis deadzone on a round gate lets a diagonal
	// through that a straight push of the same distance would not.
	//
	// THE DOMINANT AXIS WINS, AND AN EXACT DIAGONAL GOES HORIZONTAL. A stick
	// has no notion of a row, so something has to break the tie, and the tie
	// is unreachable in practice on an analogue stick and reachable on nothing
	// else. What matters is that one of the four is always the answer above the
	// deadzone: a menu that could be handed "up and left at once" would need a
	// policy for it in every client, which is the policy this function exists
	// to stop existing.
	//
	// SCREEN CONVENTION, +y DOWN, which is what gamepad.h converts to at the
	// seam and what every rectangle, viewport and camera in this engine uses.
	// So a stick pushed away from the player reports negative y and comes back
	// as `up`. Nothing above this line has to remember that.
	//
	// `deadzone` is the caller's, for the reason gamepad.h gives for never
	// choosing one: a menu comparing against a presence threshold and a player
	// walking at an analogue speed want different values. A menu wants a large
	// one - half the stick's travel is a reasonable default and is what
	// pad_direction uses - because a cursor that moves when a thumb rests on
	// the stick is worse than one that needs a deliberate push.
	Direction stick_direction(const mattmath::Vector2F& stick, float deadzone);

	// Which way a pad is being pushed, from the stick AND the d-pad.
	//
	// The two are one question to a player and two to a device, and collapsing
	// them is the line every client would otherwise write - so it is written
	// once, here, where navigation.h has been claiming it lived. The d-pad
	// wins a disagreement, because it is the deliberate input of the two: a
	// thumb resting on a stick should not fight a press.
	//
	// ONE STATE AND NOT A Gamepads, which is gamepad.h's own rule for
	// everything beside it: "free functions over two values, and Gamepads'
	// methods of the same names are forwarders to them. That is what makes the
	// whole of the edge logic testable without a device." A caller with a
	// Gamepads writes pad_direction(pads.state(slot)); a test writes the state
	// by hand and needs no controller plugged into the machine running it.
	//
	// `GamepadState` is forward-declared rather than included, so that a header
	// wanting only a Direction - engine/ui/navigation.h is the one that does -
	// does not pull the pad vocabulary in behind it.
	//
	// An absent pad reads as a neutral state rather than a stale one
	// (gamepad.h), so this answers `none` for one and needs no connected()
	// around it.
	Direction pad_direction(const GamepadState& pad, float deadzone = 0.5f);

	// A press that repeats while it is held, which is the third of the three
	// and the one clients get wrong.
	//
	// WHAT GOES WRONG WITHOUT IT, in the order people write it: reading the
	// stick raw moves the cursor once per frame, so a three-row menu is crossed
	// in a twentieth of a second and nothing can be selected; reading an edge
	// instead fixes that and leaves a menu that will not scroll a long list;
	// and adding a timer that is reset every frame the direction is held never
	// repeats at all, which looks like the edge version and is a different bug.
	// The shape that works is two intervals - a long one before the first
	// repeat, a short one between the rest - and it is worth exactly one small
	// class rather than being rewritten per client.
	//
	// IT TAKES A HELD DIRECTION AND NOT A DEVICE, so a caller may feed it a
	// stick, a d-pad, arrow keys, or all three collapsed into one. That is why
	// this is not a method on Gamepads: the keyboard wants the same repeat and
	// is not a pad.
	class DirectionRepeat
	{
	public:
		// The defaults are a menu's: four tenths of a second before the first
		// repeat, ten a second after that. Both are the caller's, because a
		// long list and a three-row page do not want the same rate.
		explicit DirectionRepeat(float first_delay = 0.4f,
			float repeat_interval = 0.1f);

		// One frame. `held` is which way the player is pushing right now, and
		// the answer is which way a cursor should move this frame - which is
		// `none` on most of them.
		//
		// A CHANGE OF DIRECTION FIRES IMMEDIATELY and restarts the delay, so
		// flicking between two rows is as responsive as pressing once. Letting
		// go is a change to `none` and arms the next press the same way.
		//
		// AT MOST ONE MOVE PER FRAME, AND THE CLOCK DOES NOT CATCH UP. A frame
		// that took a quarter of a second could in principle owe two repeats;
		// paying them would sprint the cursor after every hitch, and the return
		// type cannot express two anyway. So the interval restarts rather than
		// accumulating, and the cursor loses a step instead of gaining one.
		Direction update(Direction held, float dt);

		// Forgets what is held, so the next frame's direction is a fresh press.
		// A page that has just opened over a still-held stick calls this; a
		// cursor that jumped a row on the frame a menu appeared is the reason
		// it is public.
		void reset();

	private:
		Direction held_ = Direction::none;
		float timer_ = 0.0f;
		float first_delay_ = 0.4f;
		float repeat_interval_ = 0.1f;
	};
}

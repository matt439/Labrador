#pragma once

#include "engine/math/vector2i.h"

#include <cstdint>

namespace artattack
{
	// Fed, not read, for the reason keyboard.h gives at length and will not
	// repeat: the messages arrive at the window, so the flow is app -> input
	// and nothing in this file knows a window exists.
	//
	// THE WHEEL IS WHY THAT IS NOT MERELY A PREFERENCE HERE. Cursor position
	// and buttons both have a polling API on Windows, so a mouse built only
	// out of those could have owned a reader the way gamepads.h does. The
	// wheel has none, and cannot: it has no position to report. It exists only
	// as a stream of deltas in WM_MOUSEWHEEL, so a device that sampled instead
	// of listening would not read a stale value - it would read nothing at
	// all, and every notch turned between two samples would be gone. That one
	// channel decides the shape of the whole device, which is why the shape
	// does not match the pads' and the API deliberately still does.

	enum class MouseButton : uint8_t
	{
		// Not a button. What an unrecognised platform code translates to - the
		// extended buttons past the fifth are reported by number and there is
		// no ceiling on them - so an unknown one is dropped rather than
		// indexed.
		none = 0,

		left,
		right,
		middle,

		// The thumb buttons, in the order Windows numbers them. Named x1 and
		// x2 rather than back and forward because those are what a browser
		// decided they mean, and a game is not a browser.
		x1,
		x2,

		count,
	};

	// The mouse, one frame. A value, for the same reason GamepadState and
	// KeyboardState are: two of them are a frame boundary, and a test needs to
	// write both by hand.
	struct MouseState
	{
		// False means the window did not have the mouse this frame. Buttons
		// are all up when it is false, for the reason keyboard.h documents -
		// the button-up after a drag out of the window goes somewhere else.
		bool focused = false;

		// CLIENT PIXELS: the area the game draws into, origin at its top-left,
		// +y DOWN - the same convention every rectangle, viewport and camera
		// in this engine uses, and the same one on_window_size_changed reports
		// its size in.
		//
		// It can be outside the client rectangle, and negative: while a button
		// is held the window captures the cursor and keeps reporting it, which
		// is what makes a drag off the edge and back a single continuous
		// gesture instead of two.
		//
		// Only meaningful while `focused`. It is the last position the window
		// was told about rather than a neutral zero, because zero IS a
		// position - the top-left pixel - and parking an unfocused cursor
		// there would invent a click in the corner for anything that tested a
		// rectangle. motion() below is what stays honest across the gap.
		//
		// Turning this into world coordinates is the game's, through the
		// camera of whichever view it decides the point belongs to. The engine
		// does not choose that view: there are up to four of them on one
		// screen and one cursor, and which player owns it is policy (T1).
		mattmath::Vector2I position;

		// One bit per MouseButton, indexed by the enumerator.
		uint8_t buttons = 0;

		// NOTCHES TURNED DURING THIS FRAME, not a position - there is no such
		// thing for a wheel. Positive is away from the user (scroll up) and
		// away from the left, matching the platform's own sign.
		//
		// Fractional on a high-resolution wheel, which reports in fractions of
		// a notch rather than waiting to accumulate a whole one. A caller that
		// wants integer steps rounds; a caller scrolling a view by pixels
		// multiplies and gets smooth motion for free.
		//
		// It is a delta and not a state, so there is no is_down for it and no
		// edge function: a frame that scrolled nothing reports 0, which is the
		// same answer as a frame nobody touched the mouse in, and that is
		// correct rather than ambiguous.
		float wheel = 0.0f;
		float wheel_horizontal = 0.0f;

		// False for MouseButton::none and MouseButton::count, which are bounds
		// rather than buttons.
		bool is_down(MouseButton button) const;
	};

	static_assert(static_cast<unsigned int>(MouseButton::count) <= 8,
		"MouseState::buttons is one byte. More buttons than that need more.");

	// The edges between two frames, and the rule they inherit.
	//
	// AN EDGE REQUIRES THE WINDOW TO HAVE BEEN FOCUSED ON BOTH FRAMES,
	// identically to keyboard.h and for the same three reasons. A client
	// keying off a release asks for the focus change instead:
	//
	//     just_unfocused(now, before) && before.is_down(MouseButton::left)
	bool pressed(const MouseState& now, const MouseState& before,
		MouseButton button);
	bool released(const MouseState& now, const MouseState& before,
		MouseButton button);

	// How far the cursor moved between the two frames, in client pixels.
	//
	// Zero unless both frames were focused, which is the whole reason it is a
	// function rather than a subtraction the caller writes. The cursor is
	// wherever the desktop left it when focus comes back - possibly the other
	// side of the screen - and a camera or a slider driven by the raw
	// difference would take one enormous step on that frame.
	mattmath::Vector2I motion(const MouseState& now, const MouseState& before);

	bool just_focused(const MouseState& now, const MouseState& before);
	bool just_unfocused(const MouseState& now, const MouseState& before);

	// The mouse as it was this frame and last, and the edges between them.
	//
	// Application owns one, feeds it from the window's messages and polls it
	// once per frame before any state updates - the same arrangement, and the
	// same reason, as Gamepads and Keyboard.
	class Mouse
	{
	public:
		// THE FEEDING SIDE, public for the reasons keyboard.h gives: `input`
		// may not name a class in `app` to friend it, and a test double or a
		// replay source wants exactly this seam. The tests beside this file
		// drive a Mouse with no window anywhere.
		void on_move(const mattmath::Vector2I& position);
		void on_button_down(MouseButton button);
		void on_button_up(MouseButton button);

		// Accumulated until the next poll, not replaced. Several wheel
		// messages inside one frame is ordinary - a flicked wheel sends a
		// burst - and keeping only the last would report a nudge where the
		// player spun it.
		void on_wheel(float notches);
		void on_wheel_horizontal(float notches);

		// The window gained or lost the mouse. Losing it puts every button up
		// and drops the wheel accumulated so far; the position is left where
		// it was, because zero is a real place and pretending the cursor went
		// there is worse than admitting nothing is known.
		void set_focused(bool focused);

		// THE FRAME BOUNDARY. What was current becomes previous, what the
		// messages have been building becomes current, and the wheel
		// accumulator resets so the next frame counts from nothing.
		void poll();

		const MouseState& state() const;
		const MouseState& previous_state() const;

		bool held(MouseButton button) const;
		bool pressed(MouseButton button) const;
		bool released(MouseButton button) const;

		// Client pixels, and only meaningful while focused().
		const mattmath::Vector2I& position() const;
		mattmath::Vector2I motion() const;

		float wheel() const;
		float wheel_horizontal() const;

		bool focused() const;
		bool just_focused() const;
		bool just_unfocused() const;

	private:
		// What the messages have been building since the last poll. Its wheel
		// fields are the accumulator; every other field is the live value.
		MouseState live_;

		MouseState current_;
		MouseState previous_;
	};
}

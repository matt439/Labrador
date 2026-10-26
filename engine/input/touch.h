#pragma once

#include "engine/math/vector2i.h"

#include <cstdint>
#include <span>

namespace labrador
{
	// Fed, not read, for the reason keyboard.h gives: a touch reaches a Win32
	// program as WM_POINTER messages in a window's queue and a page as pointer
	// events on its canvas, so the flow is app -> input and nothing in this
	// file knows a window exists.
	//
	// SEVERAL POINTS AT ONCE, which is the one way this device is not the
	// mouse. A mouse has one position and a fixed set of buttons, so a bit per
	// button is its whole state. A touchscreen has as many positions as there
	// are fingers on the glass, and none of them is a button: a point is held
	// for exactly as long as it exists. So the state is a short list of points,
	// and the question a game asks of it - held, pressed, released - is asked
	// of one point by its id rather than of one button by its name.

	// One finger on the glass, in one frame.
	struct TouchPoint
	{
		// THE ENGINE'S ID, NOT THE PLATFORM'S. It is handed out when the point
		// goes down, stays the same until it comes up, and is not given to
		// another point afterwards - so a game that keeps a map from finger to
		// whatever the finger is dragging can key it on this and never find a
		// stale entry adopted by a new touch.
		//
		// The platform's own ids do not promise that. Windows and browsers both
		// reuse them, and a finger lifted and a new one put down between two
		// frames can arrive with the same number: keyed on that, a release and
		// a press would read as one point that never let go. Zero is never an
		// id, so it can stand for none.
		uint32_t id = 0;

		// CLIENT PIXELS, as MouseState::position is: origin at the top-left of
		// the area the game draws into, +y down. It can be outside the client
		// rectangle and negative, because a point that went down inside the
		// window is followed when it slides off the edge, as a dragged mouse
		// is.
		mattmath::Vector2I position;
	};

	// The touchscreen, one frame. A value, for the reason every other device
	// state is one: two of them are a frame boundary, and a test writes both
	// by hand.
	struct TouchState
	{
		// THE MOST POINTS A FRAME HOLDS, and the overflow rule that goes with
		// it: a point that goes down while this many are already down is not
		// seen at all, for the whole of its contact - not when another finger
		// lifts and makes room, because a point that appeared mid-gesture
		// would be a press nobody made at that moment. Ten is two hands, which
		// is every game this engine is for; a palm on the glass is the usual
		// way to reach it.
		static constexpr int capacity = 10;

		// False means the window is not taking touch this frame, and there are
		// no points. It is `focused` for the same reason and with the same
		// rule as KeyboardState::focused and MouseState::focused.
		bool focused = false;

		// The points down this frame, the first `count` of `points`, in the
		// order they went down.
		int count = 0;
		TouchPoint points[capacity] = {};

		// Points that were down last frame and are gone this one WITHOUT
		// having been lifted: the platform cancelled them, or the window lost
		// capture of them. A cancelled point produces no released edge, for
		// the reason Mouse::cancel_buttons gives - a gesture the system took
		// away is not a finger the player lifted, and a button that fired on
		// it would fire on something nobody let go of.
		int cancelled_count = 0;
		uint32_t cancelled[capacity] = {};

		// The point with this id, or null.
		const TouchPoint* find(uint32_t id) const;

		// Whether a point with this id is down. False for zero, which is never
		// an id.
		bool is_down(uint32_t id) const;

		// The points as a range, for the loop every caller writes.
		std::span<const TouchPoint> active() const;
	};

	// The edges between two frames, per point, and the rule they inherit.
	//
	// AN EDGE REQUIRES THE DEVICE TO HAVE BEEN FOCUSED ON BOTH FRAMES, as
	// keyboard.h states it and for its three reasons: the first frame of the
	// process, focus returning with a finger already down, and focus going
	// away with fingers down. A finger on the glass across an app switch is
	// never pressed into the game and never released into it either; a client
	// that wants to know asks for the focus change:
	//
	//     just_unfocused(now, before) && before.count > 0
	//
	// A released point is no longer in `now`, so the loop that finds one walks
	// the frame before:
	//
	//     for (const TouchPoint& point : before.active())
	//         if (released(now, before, point.id)) ...
	bool pressed(const TouchState& now, const TouchState& before, uint32_t id);
	bool released(const TouchState& now, const TouchState& before, uint32_t id);

	// How far one point moved between the two frames, in client pixels. Zero
	// unless the point is down on both and the device was focused on both, so
	// the frame a point appears on is never a jump from wherever the last one
	// was.
	mattmath::Vector2I motion(const TouchState& now, const TouchState& before,
		uint32_t id);

	bool just_focused(const TouchState& now, const TouchState& before);
	bool just_unfocused(const TouchState& now, const TouchState& before);

	// The touchscreen as it was this frame and last, and the edges between
	// them.
	//
	// A game reads the borrowed instance from Application::touch().
	// Construction, feeding and poll() belong to the shell; a game reads the
	// snapshots rather than advancing them.
	//
	// Application owns one, feeds it from the window's messages and polls it
	// once per frame beside the other devices, before any state updates.
	class Touch
	{
	public:
		// THE FEEDING SIDE, public for the reasons keyboard.h gives. The ids
		// these take are the PLATFORM'S - a Win32 pointer id or a DOM
		// pointerId - and are mapped to the engine's ids here, which is where
		// the reuse TouchPoint::id describes is absorbed.
		//
		// A point down and up again between two polls IS SEEN, for one frame,
		// pressed on that frame and released on the next. That is unlike a
		// key, which can be lost between two polls, and it is deliberate: a
		// tap is the commonest thing a finger does, and a quick one is over in
		// less than a frame often enough to matter.
		//
		// Ignored while unfocused, so nothing pressed while the window was
		// away is waiting for it when it comes back. A down for a platform id
		// that is already down ends the old point as cancelled and starts a
		// new one: its up was lost somewhere, and inventing a release would be
		// inventing input. A move, up or cancel for an id that is not down is
		// dropped, which is also what happens to every message of a point
		// that arrived past `capacity`.
		void on_down(uint32_t platform_id, const mattmath::Vector2I& position);
		void on_move(uint32_t platform_id, const mattmath::Vector2I& position);
		void on_up(uint32_t platform_id, const mattmath::Vector2I& position);
		void on_cancel(uint32_t platform_id);

		// Cancels every point, as on_cancel cancels one: gone at the next
		// poll, with no released edge. Focus is unchanged.
		void cancel_points();

		// The window gained or lost touch. Losing it removes every point, as
		// losing the foreground puts every key up, because the ups for the
		// fingers still on the glass will be delivered somewhere else. Each
		// one ends cancelled, as cancel_points ends it, so no released edge
		// reaches the game even when focus comes back before any frame saw it
		// gone - a hidden page gets no frames - and just_unfocused never fires.
		void set_focused(bool focused);

		// THE FRAME BOUNDARY, called by the shell. What was current becomes
		// previous, and what the messages have built becomes current: every
		// point still down, every point that went down and up again unseen,
		// and the ids cancelled since the last call.
		void poll();

		const TouchState& state() const;
		const TouchState& previous_state() const;

		// This frame's points, in the order they went down.
		std::span<const TouchPoint> points() const;

		bool held(uint32_t id) const;
		bool pressed(uint32_t id) const;
		bool released(uint32_t id) const;
		mattmath::Vector2I motion(uint32_t id) const;

		bool focused() const;
		bool just_focused() const;
		bool just_unfocused() const;

	private:
		// One point the messages are tracking, between polls.
		struct Contact
		{
			uint32_t platform_id = 0;
			TouchPoint point;

			// Up already, and kept only until a poll has seen it once.
			bool lifted = false;

			// In the state the last poll latched, so ending it now is an edge.
			bool latched = false;
		};

		Contact live_[TouchState::capacity] = {};
		int live_count_ = 0;
		bool live_focused_ = false;

		// Latched contacts cancelled since the last poll. Bounded by capacity
		// because a latched contact can only be cancelled once.
		uint32_t pending_cancelled_[TouchState::capacity] = {};
		int pending_cancelled_count_ = 0;

		uint32_t next_id_ = 1;

		TouchState current_;
		TouchState previous_;

		// The contact the platform id names that has not been lifted, or -1.
		int find_live(uint32_t platform_id) const;
		void remove_live(int index);
		void cancel_live(int index);
	};
}

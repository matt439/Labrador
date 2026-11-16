#include <doctest/doctest.h>
#include "engine/input/mouse.h"
#include "engine/math/vector2i.h"

#include <initializer_list>

using namespace labrador;

namespace
{
	// A mouse at a position with the named buttons down, focused. Two of these
	// are a frame boundary, which is all an edge is.
	MouseState at(int x, int y, std::initializer_list<MouseButton> down)
	{
		MouseState state;
		state.focused = true;
		state.position = labrador::Vector2I(x, y);
		for (const MouseButton button : down)
		{
			state.buttons = static_cast<uint8_t>(state.buttons |
				(1u << static_cast<unsigned int>(button)));
		}
		return state;
	}

	MouseState unfocused_at(int x, int y)
	{
		MouseState state = at(x, y, {});
		state.focused = false;
		return state;
	}

	// A Mouse focused for two frames, so edges are live.
	void settle(Mouse& mouse)
	{
		mouse.set_focused(true);
		mouse.poll();
		mouse.poll();
	}
}

TEST_SUITE("MouseTests")
{
	TEST_CASE("the positional snapshot initializer retains its original fields")
	{
		const MouseState state{ true, Vector2I(10, 20), 0, 7, 1.0f, 2.0f };
		CHECK(state.focused);
		CHECK(state.position == Vector2I(10, 20));
		CHECK(state.button_epoch == 7);
		CHECK(state.wheel == 1.0f);
		CHECK(state.wheel_horizontal == 2.0f);
		CHECK(state.pressed_buttons == 0);
		CHECK(state.released_buttons == 0);
	}

	TEST_CASE("a button is down or it is not")
	{
		const MouseState state = at(10, 20,
			{ MouseButton::left, MouseButton::x2 });

		CHECK(state.is_down(MouseButton::left));
		CHECK(state.is_down(MouseButton::x2));
		CHECK_FALSE(state.is_down(MouseButton::right));
		CHECK_FALSE(state.is_down(MouseButton::x1));
	}

	TEST_CASE("the bounds are not buttons")
	{
		// MouseButton::none is what a thumb button past the second
		// translates to, and a mouse may report any number of them. Every
		// bit is set, so only the bounds check can answer no.
		MouseState state = at(0, 0, {});
		state.buttons = 0xFF;
		state.pressed_buttons = 0xFF;
		state.released_buttons = 0xFF;

		CHECK(state.is_down(MouseButton::left));
		CHECK_FALSE(state.is_down(MouseButton::none));
		CHECK_FALSE(state.is_down(MouseButton::count));
		const MouseState before = at(0, 0, {});
		for (const MouseButton button : { MouseButton::none, MouseButton::count,
			static_cast<MouseButton>(255) })
		{
			CHECK_FALSE(pressed(state, before, button));
			CHECK_FALSE(released(state, before, button));
		}
	}

	TEST_CASE("every button gets its own bit")
	{
		const unsigned int first =
			static_cast<unsigned int>(MouseButton::none) + 1;
		const unsigned int last =
			static_cast<unsigned int>(MouseButton::count);

		for (unsigned int i = first; i < last; ++i)
		{
			const MouseButton button = static_cast<MouseButton>(i);
			const MouseState state = at(0, 0, { button });

			CHECK(state.is_down(button));

			for (unsigned int j = first; j < last; ++j)
			{
				if (i != j)
				{
					CHECK_FALSE(state.is_down(
						static_cast<MouseButton>(j)));
				}
			}
		}
	}

	TEST_CASE("an edge is down now and up last frame")
	{
		const MouseState before = at(0, 0, {});
		const MouseState now = at(0, 0, { MouseButton::left });

		CHECK(pressed(now, before, MouseButton::left));
		CHECK_FALSE(released(now, before, MouseButton::left));

		CHECK(released(before, now, MouseButton::left));
		CHECK_FALSE(pressed(before, now, MouseButton::left));
	}

	TEST_CASE("an edge requires focus on both frames")
	{
		SUBCASE("the first frame of the process")
		{
			const MouseState before;
			const MouseState now = at(0, 0, { MouseButton::left });

			CHECK_FALSE(pressed(now, before, MouseButton::left));
		}

		SUBCASE("focus leaving with a button held")
		{
			const MouseState before = at(5, 5, { MouseButton::left });
			const MouseState now = unfocused_at(5, 5);

			CHECK_FALSE(released(now, before, MouseButton::left));
			CHECK(just_unfocused(now, before));
		}
	}

	TEST_CASE("motion is zero across an unfocused frame")
	{
		// THE CURSOR IS WHEREVER THE DESKTOP LEFT IT when focus comes
		// back, so the raw difference is the distance between two
		// unrelated places. A camera or a slider driven by it takes one
		// enormous step on that frame.
		const MouseState before = unfocused_at(0, 0);
		const MouseState now = at(900, 700, {});

		CHECK(motion(now, before) == labrador::Vector2I::zero);

		// And across two focused frames it is the plain difference.
		const MouseState settled = at(100, 100, {});
		const MouseState moved = at(130, 90, {});
		CHECK(motion(moved, settled) == labrador::Vector2I(30, -10));
	}

	TEST_CASE("nothing is visible until the frame turns over")
	{
		Mouse mouse;
		settle(mouse);

		mouse.on_button_down(MouseButton::left);
		mouse.on_move(labrador::Vector2I(40, 50));
		CHECK_FALSE(mouse.held(MouseButton::left));

		mouse.poll();
		CHECK(mouse.held(MouseButton::left));
		CHECK(mouse.pressed(MouseButton::left));
		CHECK(mouse.position() == labrador::Vector2I(40, 50));

		mouse.poll();
		CHECK_FALSE(mouse.pressed(MouseButton::left));

		mouse.on_button_up(MouseButton::left);
		mouse.poll();
		CHECK(mouse.released(MouseButton::left));
	}

	TEST_CASE("a short click reports both edges once with the button up")
	{
		Mouse mouse;
		settle(mouse);
		mouse.on_button_down(MouseButton::left);
		mouse.on_button_up(MouseButton::left);
		CHECK_FALSE(mouse.pressed(MouseButton::left));
		CHECK_FALSE(mouse.released(MouseButton::left));
		mouse.poll();
		CHECK(mouse.pressed(MouseButton::left));
		CHECK(mouse.released(MouseButton::left));
		CHECK_FALSE(mouse.held(MouseButton::left));
		CHECK(pressed(mouse.state(), mouse.previous_state(), MouseButton::left));
		CHECK(released(mouse.state(), mouse.previous_state(), MouseButton::left));
		mouse.poll();
		CHECK_FALSE(mouse.pressed(MouseButton::left));
		CHECK_FALSE(mouse.released(MouseButton::left));
	}

	TEST_CASE("several clicks coalesce per button without changing the live held state")
	{
		Mouse mouse;
		settle(mouse);
		for (int click = 0; click < 3; ++click)
		{
			mouse.on_button_down(MouseButton::left);
			mouse.on_button_up(MouseButton::left);
		}
		mouse.on_button_down(MouseButton::left);
		mouse.on_button_down(MouseButton::right);
		mouse.on_button_up(MouseButton::right);
		mouse.poll();
		CHECK(mouse.pressed(MouseButton::left));
		CHECK(mouse.released(MouseButton::left));
		CHECK(mouse.held(MouseButton::left));
		CHECK(mouse.pressed(MouseButton::right));
		CHECK(mouse.released(MouseButton::right));
		CHECK_FALSE(mouse.held(MouseButton::right));
		CHECK_FALSE(mouse.pressed(MouseButton::middle));
		CHECK_FALSE(mouse.released(MouseButton::middle));
		mouse.poll();
		CHECK(mouse.held(MouseButton::left));
		CHECK_FALSE(mouse.pressed(MouseButton::left));
		CHECK_FALSE(mouse.released(MouseButton::left));
		CHECK_FALSE(mouse.pressed(MouseButton::right));
		CHECK_FALSE(mouse.released(MouseButton::right));
	}

	TEST_CASE("duplicate down and up messages add no edges")
	{
		Mouse mouse;
		settle(mouse);
		mouse.on_button_up(MouseButton::left);
		mouse.poll();
		CHECK_FALSE(mouse.released(MouseButton::left));
		mouse.on_button_down(MouseButton::left);
		mouse.poll();
		REQUIRE(mouse.pressed(MouseButton::left));
		mouse.on_button_down(MouseButton::left);
		mouse.poll();
		CHECK(mouse.held(MouseButton::left));
		CHECK_FALSE(mouse.pressed(MouseButton::left));
		mouse.on_button_up(MouseButton::left);
		mouse.poll();
		REQUIRE(mouse.released(MouseButton::left));
		mouse.on_button_up(MouseButton::left);
		mouse.poll();
		CHECK_FALSE(mouse.released(MouseButton::left));
	}

	TEST_CASE("a release and repress between polls preserves held and both edges")
	{
		Mouse mouse;
		settle(mouse);
		mouse.on_button_down(MouseButton::x2);
		mouse.poll();
		mouse.on_button_up(MouseButton::x2);
		mouse.on_button_down(MouseButton::x2);
		mouse.poll();
		CHECK(mouse.held(MouseButton::x2));
		CHECK(mouse.pressed(MouseButton::x2));
		CHECK(mouse.released(MouseButton::x2));
		mouse.poll();
		CHECK(mouse.held(MouseButton::x2));
		CHECK_FALSE(mouse.pressed(MouseButton::x2));
		CHECK_FALSE(mouse.released(MouseButton::x2));
	}

	TEST_CASE("focus loss and capture cancellation discard pending clicks")
	{
		Mouse mouse;
		settle(mouse);
		mouse.on_button_down(MouseButton::left);
		mouse.on_button_up(MouseButton::left);
		mouse.on_button_down(MouseButton::right);
		SUBCASE("focus leaves and returns before the next poll")
		{
			mouse.set_focused(false);
			mouse.set_focused(true);
		}
		SUBCASE("capture is cancelled while focus stays")
		{
			mouse.cancel_buttons();
		}
		mouse.poll();
		CHECK(mouse.focused());
		for (const MouseButton button : { MouseButton::left, MouseButton::right })
		{
			CHECK_FALSE(mouse.held(button));
			CHECK_FALSE(mouse.pressed(button));
			CHECK_FALSE(mouse.released(button));
		}
		mouse.poll();
		mouse.on_button_down(MouseButton::left);
		mouse.on_button_up(MouseButton::left);
		mouse.poll();
		CHECK(mouse.pressed(MouseButton::left));
		CHECK(mouse.released(MouseButton::left));
	}

	TEST_CASE("the wheel counts one frame and then starts again")
	{
		// It is a delta, not a state. A frame that scrolled nothing
		// reports 0 - the same answer as a frame nobody touched the mouse
		// in, which is correct rather than ambiguous.
		Mouse mouse;
		settle(mouse);

		CHECK(mouse.wheel() == 0.0f);

		// A flicked wheel sends a burst inside one frame, and keeping only
		// the last would report a nudge where the player spun it.
		mouse.on_wheel(1.0f);
		mouse.on_wheel(1.0f);
		mouse.on_wheel(0.5f);
		mouse.poll();
		CHECK(mouse.wheel() == 2.5f);

		// Nothing this frame, so nothing reported - not the running total.
		mouse.poll();
		CHECK(mouse.wheel() == 0.0f);

		mouse.on_wheel(-1.0f);
		mouse.poll();
		CHECK(mouse.wheel() == -1.0f);
	}

	TEST_CASE("the two wheel axes are independent")
	{
		Mouse mouse;
		settle(mouse);

		mouse.on_wheel(2.0f);
		mouse.on_wheel_horizontal(-3.0f);
		mouse.poll();

		CHECK(mouse.wheel() == 2.0f);
		CHECK(mouse.wheel_horizontal() == -3.0f);
	}

	TEST_CASE("losing focus puts every button up and drops the wheel")
	{
		Mouse mouse;
		settle(mouse);

		mouse.on_button_down(MouseButton::left);
		mouse.on_button_down(MouseButton::right);
		mouse.poll();
		CHECK(mouse.held(MouseButton::left));

		mouse.on_wheel(4.0f);
		mouse.set_focused(false);
		mouse.poll();

		CHECK_FALSE(mouse.held(MouseButton::left));
		CHECK_FALSE(mouse.held(MouseButton::right));
		CHECK_FALSE(mouse.focused());
		CHECK(mouse.just_unfocused());

		// Turned at this window; scrolling whatever is open when focus
		// returns would be worse than losing it.
		CHECK(mouse.wheel() == 0.0f);

		// And no phantom release, per the both-frames rule.
		CHECK_FALSE(mouse.released(MouseButton::left));
	}

	TEST_CASE("the position survives losing focus")
	{
		// Zero IS a position - the top-left pixel - so parking an
		// unfocused cursor there would invent a click in the corner for
		// anything testing a rectangle. The last known place is kept and
		// motion() is what stays honest across the gap.
		Mouse mouse;
		settle(mouse);

		mouse.on_move(labrador::Vector2I(300, 200));
		mouse.poll();

		mouse.set_focused(false);
		mouse.poll();

		CHECK(mouse.position() == labrador::Vector2I(300, 200));
		CHECK(mouse.motion() == labrador::Vector2I::zero);
	}

	TEST_CASE("a position outside the client area is kept as given")
	{
		// While a button is captured the cursor can be left of or above
		// the window, which is a negative coordinate. Anything that
		// treated the words as unsigned would report roughly 65,000
		// instead, and a drag off the left edge would fly off the right.
		Mouse mouse;
		settle(mouse);

		mouse.on_move(labrador::Vector2I(-12, -40));
		mouse.poll();

		CHECK(mouse.position() == labrador::Vector2I(-12, -40));
	}

	TEST_CASE("an unrecognised button changes nothing")
	{
		Mouse mouse;
		settle(mouse);

		mouse.on_button_down(MouseButton::none);
		mouse.on_button_down(MouseButton::count);
		mouse.poll();

		CHECK(mouse.state().buttons == 0);
	}
}

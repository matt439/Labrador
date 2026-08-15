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
		state.position = mattmath::Vector2I(x, y);
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

namespace MouseTests
{
	TEST_SUITE("MouseTests")
	{
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
			// translates to, and a mouse may report any number of them.
			const MouseState state = at(0, 0, { MouseButton::left });

			CHECK_FALSE(state.is_down(MouseButton::none));
			CHECK_FALSE(state.is_down(MouseButton::count));
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

			CHECK(motion(now, before) == mattmath::Vector2I::ZERO);

			// And across two focused frames it is the plain difference.
			const MouseState settled = at(100, 100, {});
			const MouseState moved = at(130, 90, {});
			CHECK(motion(moved, settled) == mattmath::Vector2I(30, -10));
		}

		TEST_CASE("nothing is visible until the frame turns over")
		{
			Mouse mouse;
			settle(mouse);

			mouse.on_button_down(MouseButton::left);
			mouse.on_move(mattmath::Vector2I(40, 50));
			CHECK_FALSE(mouse.held(MouseButton::left));

			mouse.poll();
			CHECK(mouse.held(MouseButton::left));
			CHECK(mouse.pressed(MouseButton::left));
			CHECK(mouse.position() == mattmath::Vector2I(40, 50));

			mouse.poll();
			CHECK_FALSE(mouse.pressed(MouseButton::left));

			mouse.on_button_up(MouseButton::left);
			mouse.poll();
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

			mouse.on_move(mattmath::Vector2I(300, 200));
			mouse.poll();

			mouse.set_focused(false);
			mouse.poll();

			CHECK(mouse.position() == mattmath::Vector2I(300, 200));
			CHECK(mouse.motion() == mattmath::Vector2I::ZERO);
		}

		TEST_CASE("a position outside the client area is kept as given")
		{
			// While a button is captured the cursor can be left of or above
			// the window, which is a negative coordinate. Anything that
			// treated the words as unsigned would report roughly 65,000
			// instead, and a drag off the left edge would fly off the right.
			Mouse mouse;
			settle(mouse);

			mouse.on_move(mattmath::Vector2I(-12, -40));
			mouse.poll();

			CHECK(mouse.position() == mattmath::Vector2I(-12, -40));
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
}

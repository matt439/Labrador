#include <doctest/doctest.h>

#include "engine/input/direction.h"
#include "engine/input/gamepad.h"
#include "engine/math/vector2f.h"

#include <initializer_list>

using namespace labrador;
using mattmath::Vector2F;

// The producer engine/ui/navigation.h spent its whole life claiming existed.
//
// That header said `Direction` was "produced by the input module from a stick
// or a d-pad" and `grep -rn Direction engine/ | grep -v engine/ui/` was empty.
// docs/next.md section 3.3 is the finding. What a client wrote in its place
// was a deadzone, a quadrant test and a hold-to-repeat, and the third is the
// one that goes wrong - so it is the one with the most cases here.
//
// NO DEVICE, AND THAT IS gamepad.h's RULE RATHER THAN THIS FILE'S PREFERENCE:
// the edge functions are free functions over values so that "a test writes the
// two frames by hand rather than needing a controller to be plugged into the
// machine running it". pad_direction is one of those, and DirectionRepeat is
// fed a Direction rather than a device for the same reason.
namespace
{
	// A pad with the named buttons down and nothing else, copied from
	// gamepad_tests.cpp because it is the same three lines and importing a
	// fixture header for them would be the heavier of the two.
	GamepadState pad(std::initializer_list<GamepadButton> down)
	{
		GamepadState state;
		state.connected = true;

		for (const GamepadButton button : down)
		{
			state.buttons = static_cast<uint16_t>(state.buttons |
				(1u << static_cast<unsigned int>(button)));
		}

		return state;
	}

	GamepadState pad_with_stick(const Vector2F& stick)
	{
		GamepadState state;
		state.connected = true;
		state.left_stick = stick;

		return state;
	}
}

namespace DirectionTests
{
	TEST_SUITE("DirectionTests")
	{
		TEST_CASE("a stick inside the deadzone points nowhere")
		{
			CHECK(stick_direction(Vector2F::ZERO, 0.5f) == Direction::none);
			CHECK(stick_direction(Vector2F(0.4f, 0.0f), 0.5f) ==
				Direction::none);
			CHECK(stick_direction(Vector2F(0.0f, -0.49f), 0.5f) ==
				Direction::none);

			// Radially, which is the whole reason apply_deadzone has a vector
			// overload: a diagonal at 0.4 in each axis is 0.57 long and would
			// pass a per-axis gate of 0.5 in neither axis while passing a
			// square one in both. It is outside a round gate of 0.5, so it
			// reports a direction.
			CHECK(stick_direction(Vector2F(0.4f, 0.4f), 0.5f) !=
				Direction::none);
		}

		TEST_CASE("a stick points the way it is pushed, in screen convention")
		{
			// +y is DOWN, which gamepad.h converts to at the seam. A stick
			// pushed away from the player is negative y and is `up`.
			CHECK(stick_direction(Vector2F(0.0f, -1.0f), 0.5f) ==
				Direction::up);
			CHECK(stick_direction(Vector2F(0.0f, 1.0f), 0.5f) ==
				Direction::down);
			CHECK(stick_direction(Vector2F(-1.0f, 0.0f), 0.5f) ==
				Direction::left);
			CHECK(stick_direction(Vector2F(1.0f, 0.0f), 0.5f) ==
				Direction::right);
		}

		TEST_CASE("the dominant axis wins, and a dead heat goes horizontal")
		{
			CHECK(stick_direction(Vector2F(0.9f, -0.3f), 0.2f) ==
				Direction::right);
			CHECK(stick_direction(Vector2F(-0.3f, 0.9f), 0.2f) ==
				Direction::down);

			// The tie-break, which is unreachable on a real analogue stick and
			// is pinned so that it cannot drift into "sometimes up".
			CHECK(stick_direction(Vector2F(0.7f, 0.7f), 0.2f) ==
				Direction::right);
			CHECK(stick_direction(Vector2F(-0.7f, -0.7f), 0.2f) ==
				Direction::left);
		}

		TEST_CASE("a pad reads its d-pad and its stick as one question")
		{
			CHECK(pad_direction(pad({ GamepadButton::dpad_up })) ==
				Direction::up);
			CHECK(pad_direction(pad({ GamepadButton::dpad_left })) ==
				Direction::left);
			CHECK(pad_direction(pad_with_stick(Vector2F(0.0f, 0.9f))) ==
				Direction::down);

			SUBCASE("the d-pad wins a disagreement")
			{
				GamepadState both = pad({ GamepadButton::dpad_up });
				both.left_stick = Vector2F(0.0f, 0.9f);

				CHECK(pad_direction(both) == Direction::up);
			}

			SUBCASE("an absent pad is neutral rather than stale")
			{
				// A default GamepadState is what an empty slot reports
				// (gamepad.h), so this needs no connected() around it.
				CHECK(pad_direction(GamepadState()) == Direction::none);
			}

			SUBCASE("a pad at rest points nowhere")
			{
				CHECK(pad_direction(pad({})) == Direction::none);
			}
		}

		// The third of the three, and the one clients get wrong.
		TEST_CASE("a repeat fires once, waits, then repeats")
		{
			DirectionRepeat repeat(0.4f, 0.1f);

			SUBCASE("a fresh press fires on the frame it arrives")
			{
				CHECK(repeat.update(Direction::up, 1.0f / 60.0f) ==
					Direction::up);
			}

			SUBCASE("and holding does not fire again before the first delay")
			{
				REQUIRE(repeat.update(Direction::up, 1.0f / 60.0f) ==
					Direction::up);

				// Twenty frames is a third of a second, short of 0.4.
				for (int frame = 0; frame < 20; ++frame)
				{
					CHECK(repeat.update(Direction::up, 1.0f / 60.0f) ==
						Direction::none);
				}
			}

			SUBCASE("then it fires again, and again at the shorter interval")
			{
				REQUIRE(repeat.update(Direction::up, 0.0f) == Direction::up);

				// Past the first delay in one step.
				CHECK(repeat.update(Direction::up, 0.5f) == Direction::up);

				// Not yet past the repeat interval.
				CHECK(repeat.update(Direction::up, 0.05f) == Direction::none);
				// And now past it.
				CHECK(repeat.update(Direction::up, 0.06f) == Direction::up);
			}
		}

		TEST_CASE("a held direction does not silently repeat every frame")
		{
			// The bug this class exists to stop: reading a stick raw crosses a
			// three-row menu in a twentieth of a second. Sixty frames of
			// holding must produce far fewer than sixty moves.
			DirectionRepeat repeat;

			int moves = 0;

			for (int frame = 0; frame < 60; ++frame)
			{
				if (repeat.update(Direction::down, 1.0f / 60.0f) !=
					Direction::none)
				{
					++moves;
				}
			}

			// One second: the press, then repeats after 0.4 at one every 0.1.
			CHECK(moves > 1);
			CHECK(moves < 10);
		}

		TEST_CASE("changing direction fires at once and restarts the delay")
		{
			DirectionRepeat repeat(0.4f, 0.1f);

			REQUIRE(repeat.update(Direction::up, 0.0f) == Direction::up);
			REQUIRE(repeat.update(Direction::up, 0.3f) == Direction::none);

			// Flicking to the other row is as responsive as a first press.
			CHECK(repeat.update(Direction::down, 0.0f) == Direction::down);

			// And the long delay applies again rather than the short one - the
			// 0.3 already spent on `up` does not carry over.
			CHECK(repeat.update(Direction::down, 0.2f) == Direction::none);
			CHECK(repeat.update(Direction::down, 0.3f) == Direction::down);
		}

		TEST_CASE("letting go fires nothing and arms the next press")
		{
			DirectionRepeat repeat(0.4f, 0.1f);

			REQUIRE(repeat.update(Direction::up, 0.0f) == Direction::up);

			CHECK(repeat.update(Direction::none, 0.016f) == Direction::none);
			CHECK(repeat.update(Direction::none, 1.0f) == Direction::none);

			// The next press is a first press, not a repeat.
			CHECK(repeat.update(Direction::up, 0.0f) == Direction::up);
			CHECK(repeat.update(Direction::up, 0.2f) == Direction::none);
		}

		// The decision the header states: the clock restarts rather than
		// accumulating, so a hitch costs a step instead of buying several.
		TEST_CASE("a long frame does not owe the cursor a burst")
		{
			DirectionRepeat repeat(0.4f, 0.1f);

			REQUIRE(repeat.update(Direction::up, 0.0f) == Direction::up);

			// A quarter of a second in one frame, which is four repeats' worth.
			CHECK(repeat.update(Direction::up, 0.65f) == Direction::up);

			// The next one is a full interval away, not immediate.
			CHECK(repeat.update(Direction::up, 0.05f) == Direction::none);
			CHECK(repeat.update(Direction::up, 0.06f) == Direction::up);
		}

		TEST_CASE("reset makes the next frame a fresh press")
		{
			DirectionRepeat repeat(0.4f, 0.1f);

			REQUIRE(repeat.update(Direction::up, 0.0f) == Direction::up);
			REQUIRE(repeat.update(Direction::up, 0.3f) == Direction::none);

			// A menu opening over a stick that is already held: without this
			// the cursor is off the first row before the screen is visible.
			repeat.reset();

			CHECK(repeat.update(Direction::up, 0.0f) == Direction::up);
			CHECK(repeat.update(Direction::up, 0.2f) == Direction::none);
		}
	}
}

#include <doctest/doctest.h>
#include "engine/input/gamepad.h"
using namespace artattack;

namespace
{
	// A pad with the named buttons down and nothing else. Two of these are a
	// frame boundary, which is all an edge is - and writing them by hand is
	// why the edge logic lives in free functions rather than behind the
	// reader.
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

	GamepadState pad_with_triggers(float left, float right)
	{
		GamepadState state;
		state.connected = true;
		state.left_trigger = left;
		state.right_trigger = right;
		return state;
	}
}

namespace GamepadTests
{
	TEST_SUITE("GamepadTests")
	{
		TEST_CASE("a button is down or it is not")
		{
			const GamepadState state = pad({ GamepadButton::a,
				GamepadButton::dpad_left });

			CHECK(state.is_down(GamepadButton::a));
			CHECK(state.is_down(GamepadButton::dpad_left));
			CHECK_FALSE(state.is_down(GamepadButton::b));
			CHECK_FALSE(state.is_down(GamepadButton::dpad_right));
		}

		TEST_CASE("every button gets its own bit")
		{
			// One bit each in a uint16_t, and there are fourteen of them, so a
			// shift that overflowed would alias two buttons onto one.
			const GamepadButton all[] = {
				GamepadButton::a, GamepadButton::b, GamepadButton::x,
				GamepadButton::y, GamepadButton::left_shoulder,
				GamepadButton::right_shoulder, GamepadButton::left_stick,
				GamepadButton::right_stick, GamepadButton::back,
				GamepadButton::start, GamepadButton::dpad_up,
				GamepadButton::dpad_down, GamepadButton::dpad_left,
				GamepadButton::dpad_right,
			};

			for (const GamepadButton one : all)
			{
				const GamepadState state = pad({ one });
				for (const GamepadButton other : all)
				{
					CHECK(state.is_down(other) == (other == one));
				}
			}
		}

		TEST_CASE("a press is the frame it went down and no other")
		{
			const GamepadState up = pad({});
			const GamepadState down = pad({ GamepadButton::a });

			CHECK(pressed(down, up, GamepadButton::a));
			CHECK_FALSE(pressed(down, down, GamepadButton::a));
			CHECK_FALSE(pressed(up, down, GamepadButton::a));
			CHECK_FALSE(pressed(up, up, GamepadButton::a));
		}

		TEST_CASE("a release is the frame it came back up")
		{
			const GamepadState up = pad({});
			const GamepadState down = pad({ GamepadButton::start });

			CHECK(released(up, down, GamepadButton::start));
			CHECK_FALSE(released(down, up, GamepadButton::start));
			CHECK_FALSE(released(down, down, GamepadButton::start));
			CHECK_FALSE(released(up, up, GamepadButton::start));
		}

		TEST_CASE("a button held across a transition is not a press")
		{
			// This is the bug the two hand-primed edge detectors kept having:
			// the A that opened a menu is still down on the menu's first
			// frame. It is not a press there, because the engine polled the
			// pad on that earlier frame too.
			const GamepadState down = pad({ GamepadButton::a });

			CHECK_FALSE(pressed(down, down, GamepadButton::a));
		}

		TEST_CASE("the frame a pad appears on is not a press")
		{
			// The absence is the whole point: `before` is what a slot reads as
			// on the process's first poll, on the frame after a replug, and on
			// the frame after any other absence the reader reports. A pad that
			// arrives holding A did not press A here.
			const GamepadState absent;
			const GamepadState arrives_holding_a = pad({ GamepadButton::a });

			CHECK_FALSE(pressed(arrives_holding_a, absent, GamepadButton::a));

			// And the guard a careful client would reach for does not help,
			// which is why this belongs in the engine: the slot IS connected on
			// the offending frame.
			CHECK(arrives_holding_a.connected);
			CHECK(just_connected(arrives_holding_a, absent));
		}

		TEST_CASE("a pad that vanishes holding a button did not release it")
		{
			const GamepadState absent;
			const GamepadState down = pad({ GamepadButton::start });

			CHECK_FALSE(released(absent, down, GamepadButton::start));

			// The information is not lost, only spelt differently - this is the
			// idiom gamepad.h names for a client that keyed "stop" off the
			// release.
			CHECK(just_disconnected(absent, down));
			CHECK(down.is_down(GamepadButton::start));
		}

		TEST_CASE("an edge between two occupied frames is unaffected")
		{
			// The regression guard for the conjunction above: it must gate on
			// presence and nothing else.
			const GamepadState up = pad({});
			const GamepadState down = pad({ GamepadButton::a });

			CHECK(pressed(down, up, GamepadButton::a));
			CHECK(released(up, down, GamepadButton::a));
		}

		TEST_CASE("occupancy is not identity, and the rule does not claim to be")
		{
			// Two connected frames are two connected frames. gamepads.h says
			// this engine does not track device identity, so a pad leaving a
			// slot and a different one landing in it between polls is still an
			// edge here - there is no absent frame for the rule to catch, and
			// pretending otherwise is the promise it exists to stop making.
			const GamepadState one_pad = pad({});
			const GamepadState another_pad = pad({ GamepadButton::x });

			CHECK(pressed(another_pad, one_pad, GamepadButton::x));
			CHECK_FALSE(just_connected(another_pad, one_pad));
		}

		TEST_CASE("a trigger edge needs the slot occupied on both frames too")
		{
			const GamepadState absent;
			const GamepadState pulled = pad_with_triggers(1.0f, 0.0f);

			CHECK_FALSE(trigger_pressed(pulled, absent,
				GamepadTrigger::left, 0.6f));
			CHECK_FALSE(trigger_released(absent, pulled,
				GamepadTrigger::left, 0.6f));

			// Still an edge when the slot was occupied for both.
			const GamepadState off = pad_with_triggers(0.0f, 0.0f);
			CHECK(trigger_pressed(pulled, off, GamepadTrigger::left, 0.6f));
			CHECK(trigger_released(off, pulled, GamepadTrigger::left, 0.6f));
		}

		TEST_CASE("a trigger edge is measured against the caller's threshold")
		{
			const GamepadState off = pad_with_triggers(0.0f, 0.0f);
			const GamepadState half = pad_with_triggers(0.5f, 0.0f);
			const GamepadState full = pad_with_triggers(1.0f, 0.0f);

			CHECK_FALSE(trigger_held(half, GamepadTrigger::left, 0.6f));
			CHECK(trigger_held(half, GamepadTrigger::left, 0.4f));

			CHECK(trigger_pressed(full, off, GamepadTrigger::left, 0.6f));
			CHECK(trigger_pressed(full, half, GamepadTrigger::left, 0.6f));
			CHECK_FALSE(trigger_pressed(full, full, GamepadTrigger::left, 0.6f));

			// The same two frames, read against a lower threshold, are not an
			// edge at all - both are already past it.
			CHECK_FALSE(trigger_pressed(full, half, GamepadTrigger::left, 0.4f));

			CHECK(trigger_released(half, full, GamepadTrigger::left, 0.6f));
			CHECK_FALSE(trigger_released(full, half, GamepadTrigger::left, 0.6f));
		}

		TEST_CASE("the two triggers are told apart")
		{
			const GamepadState state = pad_with_triggers(1.0f, 0.0f);

			CHECK(state.trigger(GamepadTrigger::left) == doctest::Approx(1.0f));
			CHECK(state.trigger(GamepadTrigger::right) == doctest::Approx(0.0f));
			CHECK(trigger_held(state, GamepadTrigger::left, 0.6f));
			CHECK_FALSE(trigger_held(state, GamepadTrigger::right, 0.6f));
		}

		TEST_CASE("connecting and disconnecting are edges too")
		{
			GamepadState absent;
			const GamepadState present = pad({});

			CHECK(just_connected(present, absent));
			CHECK_FALSE(just_connected(present, present));
			CHECK_FALSE(just_connected(absent, absent));

			CHECK(just_disconnected(absent, present));
			CHECK_FALSE(just_disconnected(absent, absent));
			CHECK_FALSE(just_disconnected(present, present));
		}

		TEST_CASE("an absent pad reads as neutral rather than as stale")
		{
			const GamepadState absent;

			CHECK_FALSE(absent.connected);
			CHECK_FALSE(absent.is_down(GamepadButton::a));
			CHECK(absent.trigger(GamepadTrigger::left) == doctest::Approx(0.0f));
			CHECK(absent.left_stick == mattmath::Vector2F::ZERO);
			CHECK(absent.right_stick == mattmath::Vector2F::ZERO);
		}
	}
}

#include <doctest/doctest.h>

#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/scripted_gamepad_source.h"

#include <limits>
#include <stdexcept>
#include <utility>

using namespace labrador;

namespace
{
	GamepadState connected_pad(uint16_t buttons = 0)
	{
		GamepadState state;
		state.connected = true;
		state.buttons = buttons;
		state.left_stick = Vector2F(0.25f, -0.75f);
		return state;
	}

	int count_records(const ScriptedGamepadSource& source,
		ScriptedGamepadEvent event)
	{
		int count = 0;
		for (const ScriptedGamepadRecord& record : source.records())
		{
			if (record.event == event)
			{
				++count;
			}
		}
		return count;
	}

	constexpr Vibration request{ 0.25f, 0.75f, 0.5f };
}

TEST_CASE("scripted pads copy pending snapshots and use real Gamepads edges")
{
	ScriptedGamepadSource source;
	GamepadReader reader(source);
	Gamepads pads(&reader);
	GamepadState next = connected_pad();
	source.set_state(2, next);
	next.buttons = gamepad_button_bit(GamepadButton::a);
	CHECK_FALSE(reader.read(2).connected);
	pads.poll();
	CHECK(pads.just_connected(2));
	CHECK_FALSE(pads.held(2, GamepadButton::a));
	CHECK(pads.state(2).left_stick == Vector2F(0.25f, -0.75f));
	source.set_state(2, next);
	pads.poll();
	CHECK(pads.pressed(2, GamepadButton::a));
	pads.poll();
	CHECK_FALSE(pads.pressed(2, GamepadButton::a));
	next.buttons = 0;
	source.set_state(2, next);
	pads.poll();
	CHECK(pads.released(2, GamepadButton::a));
	next.connected = false;
	next.buttons = gamepad_button_bit(GamepadButton::b);
	source.set_state(2, next);
	pads.poll();
	CHECK(pads.just_disconnected(2));
	CHECK(pads.state(2).buttons == 0);
	CHECK(pads.state(2).left_stick == Vector2F::zero);
	source.set_state(2, connected_pad(gamepad_button_bit(GamepadButton::b)));
	pads.poll();
	CHECK(pads.just_connected(2));
	CHECK_FALSE(pads.pressed(2, GamepadButton::b));
	CHECK_FALSE(reader.read(-1).connected);
	CHECK_FALSE(reader.read(Gamepads::max_count).connected);
	CHECK_THROWS_AS(source.set_state(-1, next), std::out_of_range);
	CHECK_THROWS_AS(source.set_vibration_available(4, true), std::out_of_range);
}

TEST_CASE("scripted vibration expires and records refusal stop disconnect and teardown")
{
	ScriptedGamepadSource source;
	{
		GamepadReader reader(source);
		Gamepads pads(&reader);
		source.set_state(0, connected_pad());
		pads.poll();
		CHECK(pads.vibrate(0, request) == VibrationResult::submitted);
		REQUIRE(source.records().back().event == ScriptedGamepadEvent::vibrate);
		CHECK(source.records().back().accepted);
		CHECK(source.records().back().vibration.high_frequency == 0.75f);
		source.set_time(0.49);
		pads.poll();
		CHECK(count_records(source, ScriptedGamepadEvent::stop) == 0);
		source.set_time(0.5);
		pads.poll();
		CHECK(count_records(source, ScriptedGamepadEvent::stop) == 1);
		CHECK(source.records().back().seconds == 0.5);
		CHECK(pads.vibrate(0, request) == VibrationResult::submitted);
		source.set_vibration_available(0, false);
		CHECK(pads.vibrate(0, request) == VibrationResult::unavailable);
		CHECK(count_records(source, ScriptedGamepadEvent::stop) == 2);
		REQUIRE(source.records().size() >= 2);
		CHECK_FALSE(source.records()[source.records().size() - 2].accepted);
		source.set_vibration_available(0, true);
		CHECK(pads.vibrate(0, request) == VibrationResult::submitted);
		source.set_state(0, GamepadState());
		pads.poll();
		CHECK(count_records(source, ScriptedGamepadEvent::stop) == 3);
		CHECK(pads.vibrate(0, request) == VibrationResult::unavailable);
		source.set_state(0, connected_pad());
		pads.poll();
		pads.stop_vibration(0);
		CHECK(count_records(source, ScriptedGamepadEvent::stop) == 4);
		CHECK(pads.vibrate(0, request) == VibrationResult::submitted);
	}
	CHECK(count_records(source, ScriptedGamepadEvent::stop) == 5);
	CHECK_FALSE(source.records_overflowed());
}

TEST_CASE("scripted suspension discards output and reconnects without a button edge")
{
	ScriptedGamepadSource source;
	GamepadReader reader(source);
	Gamepads pads(&reader);
	source.set_state(0, connected_pad());
	pads.poll();
	CHECK(pads.vibrate(0, request) == VibrationResult::submitted);
	reader.suspend();
	CHECK(count_records(source, ScriptedGamepadEvent::stop) == 1);
	CHECK_FALSE(reader.vibrate(0, request));
	source.set_state(0, connected_pad(gamepad_button_bit(GamepadButton::a)));
	pads.poll();
	CHECK_FALSE(pads.connected(0));
	reader.resume();
	pads.poll();
	CHECK(pads.just_connected(0));
	CHECK_FALSE(pads.pressed(0, GamepadButton::a));
	CHECK(count_records(source, ScriptedGamepadEvent::suspend) == 1);
	CHECK(count_records(source, ScriptedGamepadEvent::resume) == 1);
	CHECK(count_records(source, ScriptedGamepadEvent::vibrate) == 2);
}

TEST_CASE("scripted readers transfer one borrow and stop the move destination")
{
	GamepadReader shipping;
	ScriptedGamepadSource first;
	ScriptedGamepadSource second;
	first.set_state(0, connected_pad());
	second.set_state(1, connected_pad());
	{
		GamepadReader original(first);
		CHECK_THROWS_AS(static_cast<void>(GamepadReader(first)), std::logic_error);
		original.sample();
		CHECK(original.read(0).connected);
		CHECK(original.vibrate(0, request));
		GamepadReader moved(std::move(original));
		CHECK(moved.read(0).connected);
		GamepadReader destination(second);
		destination.sample();
		CHECK_FALSE(destination.read(0).connected);
		CHECK(destination.read(1).connected);
		CHECK(destination.vibrate(1, request));
		destination = std::move(moved);
		CHECK(count_records(second, ScriptedGamepadEvent::stop) == 1);
		CHECK(destination.read(0).connected);
		GamepadReader replacement(second);
		CHECK_FALSE(replacement.read(1).connected);
		replacement.sample();
		CHECK(replacement.read(1).connected);
	}
	CHECK(count_records(first, ScriptedGamepadEvent::stop) == 1);
	GamepadReader reattached(first);
	CHECK_FALSE(reattached.read(0).connected);
	reattached.sample();
	CHECK(reattached.read(0).connected);
	CHECK_FALSE(first.records_overflowed());
	CHECK_FALSE(second.records_overflowed());
}

TEST_CASE("scripted time and bounded records have explicit failure behaviour")
{
	ScriptedGamepadSource source(2);
	GamepadReader reader(source);
	CHECK_THROWS_AS(source.set_time(-0.1), std::invalid_argument);
	CHECK_THROWS_AS(source.set_time(std::numeric_limits<double>::infinity()),
		std::invalid_argument);
	CHECK_THROWS_AS(source.set_time(std::numeric_limits<double>::quiet_NaN()),
		std::invalid_argument);
	source.set_time(2.0);
	CHECK_THROWS_AS(source.set_time(1.0), std::invalid_argument);
	source.set_state(0, connected_pad());
	reader.sample();
	CHECK(reader.vibrate(0, request));
	reader.suspend();
	CHECK(source.records().size() == 2);
	CHECK(source.records_overflowed());
	source.clear_records();
	CHECK(source.records().empty());
	CHECK_FALSE(source.records_overflowed());
	reader.resume();
	reader.sample();
	CHECK(reader.read(0).connected);
}

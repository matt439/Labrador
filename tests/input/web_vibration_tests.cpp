#include <doctest/doctest.h>

#include "engine/input/gamepad.h"
#include "engine/input/gamepad_reader.h"

#include <emscripten/em_js.h>

using namespace labrador;

// The browser's vibration transport, driven through GamepadReader against a
// fake navigator.getGamepads() under Node: one standard pad whose
// vibrationActuator records what it was asked. vibration_tests.cpp holds the
// lifetime rules against a recording transport; this file holds that
// engine/input/web/gamepad_reader.cpp turns them into the Gamepad API's
// "dual-rumble" effect and reset() calls. No case here turns a motor or
// says what a browser does with the effect. Compiled into the browser build
// only, beside standard_gamepad_tests.cpp.

// Replaces Node's navigator with one holding a single standard pad at browser
// index 0, keeping the original to put back. `dual_rumble` is whether the
// actuator lists that effect; one that lists only another is a pad that will
// not rumble.
EM_JS(void, test_install_fake_pad, (int dual_rumble), {
	const fake = {
		log: [],
		saved: Object.getOwnPropertyDescriptor(globalThis, "navigator"),
	};
	const actuator = {
		effects: dual_rumble ? ["dual-rumble"] : ["trigger-rumble"],
		playEffect(type, params) {
			fake.log.push({ kind: "play", type: type, params: params });
			return Promise.resolve("complete");
		},
		reset() {
			fake.log.push({ kind: "reset" });
			return Promise.resolve("complete");
		},
	};
	const buttons = [];
	for (let i = 0; i < 17; ++i) {
		buttons.push({ pressed: false, touched: false, value: 0 });
	}
	const pad = {
		id: "Fake pad (STANDARD GAMEPAD)",
		index: 0,
		connected: true,
		mapping: "standard",
		timestamp: 0,
		axes: [0, 0, 0, 0],
		buttons: buttons,
		vibrationActuator: actuator,
	};
	Object.defineProperty(globalThis, "navigator", {
		value: { getGamepads: () => [pad] },
		configurable: true,
		writable: true,
	});
	globalThis.labradorFakePad = fake;
});

EM_JS(void, test_remove_fake_pad, (), {
	const fake = globalThis.labradorFakePad;
	if (fake.saved) {
		Object.defineProperty(globalThis, "navigator", fake.saved);
	} else {
		delete globalThis.navigator;
	}
	delete globalThis.labradorFakePad;
});

EM_JS(int, test_call_count, (), {
	return globalThis.labradorFakePad.log.length;
});

// 1 for a dual-rumble playEffect, 2 for reset, 0 for anything else.
EM_JS(int, test_call_kind, (int call), {
	const entry = globalThis.labradorFakePad.log[call];
	if (entry.kind === "reset") {
		return 2;
	}
	return entry.kind === "play" && entry.type === "dual-rumble" ? 1 : 0;
});

// One of the effect's parameters: 0 strongMagnitude, 1 weakMagnitude,
// 2 duration, 3 startDelay.
EM_JS(double, test_call_parameter, (int call, int which), {
	const params = globalThis.labradorFakePad.log[call].params;
	return [params.strongMagnitude, params.weakMagnitude, params.duration,
		params.startDelay][which];
});

namespace
{
	constexpr int play = 1;
	constexpr int reset = 2;

	// The fake pad for one case's length, so no other case in the suite
	// sees a Gamepad API: gamepad_tests.cpp expects Node to have none.
	struct FakePad
	{
		explicit FakePad(bool dual_rumble)
		{
			test_install_fake_pad(dual_rumble ? 1 : 0);
		}

		~FakePad()
		{
			test_remove_fake_pad();
		}

		FakePad(const FakePad&) = delete;
		FakePad& operator=(const FakePad&) = delete;
	};

	// A reader that has sampled once, so the fake pad holds slot 0.
	void sample_once(GamepadReader& reader)
	{
		reader.sample();
		REQUIRE(reader.read(0).connected);
	}
}

TEST_CASE("the browser plays dual-rumble with the requested strengths, and resets on stop")
{
	const FakePad fake(true);
	GamepadReader reader;
	sample_once(reader);

	// low_frequency is the heavy motor, which the browser calls strong.
	CHECK(reader.vibrate(0, Vibration{ 0.25f, 0.75f, 0.5f }));
	REQUIRE(test_call_count() == 1);
	CHECK(test_call_kind(0) == play);
	CHECK(test_call_parameter(0, 0) == doctest::Approx(0.25));
	CHECK(test_call_parameter(0, 1) == doctest::Approx(0.75));
	CHECK(test_call_parameter(0, 2) == doctest::Approx(500.0));
	CHECK(test_call_parameter(0, 3) == doctest::Approx(0.0));

	reader.stop_vibration(0);
	REQUIRE(test_call_count() == 2);
	CHECK(test_call_kind(1) == reset);
}

TEST_CASE("a slot with no browser pad sends the browser nothing")
{
	const FakePad fake(true);
	GamepadReader reader;
	sample_once(reader);

	CHECK_FALSE(reader.vibrate(1, Vibration{ 1.0f, 1.0f, 1.0f }));
	reader.stop_vibration(1);
	CHECK(test_call_count() == 0);
}

TEST_CASE("suspension resets a playing pad and plays nothing until resume")
{
	const FakePad fake(true);
	GamepadReader reader;
	sample_once(reader);

	CHECK(reader.vibrate(0, Vibration{ 1.0f, 0.5f, 2.0f }));
	reader.suspend();
	REQUIRE(test_call_count() == 2);
	CHECK(test_call_kind(1) == reset);

	CHECK_FALSE(reader.vibrate(0, Vibration{ 1.0f, 0.5f, 2.0f }));
	CHECK(test_call_count() == 2);

	// Resume replays nothing; the next request plays.
	reader.resume();
	CHECK(test_call_count() == 2);
	sample_once(reader);
	CHECK(reader.vibrate(0, Vibration{ 1.0f, 0.5f, 2.0f }));
	REQUIRE(test_call_count() == 3);
	CHECK(test_call_kind(2) == play);
}

TEST_CASE("destroying the reader resets a pad it vibrated, and only then")
{
	const FakePad fake(true);
	{
		GamepadReader reader;
		sample_once(reader);
	}
	CHECK(test_call_count() == 0);

	{
		GamepadReader reader;
		sample_once(reader);
		CHECK(reader.vibrate(0, Vibration{ 0.5f, 0.5f, 1.0f }));
	}
	REQUIRE(test_call_count() == 2);
	CHECK(test_call_kind(1) == reset);
}

TEST_CASE("an actuator without dual-rumble is unavailable output")
{
	const FakePad fake(false);
	GamepadReader reader;
	sample_once(reader);

	CHECK_FALSE(reader.vibrate(0, Vibration{ 1.0f, 1.0f, 1.0f }));
	CHECK(test_call_count() == 0);
}

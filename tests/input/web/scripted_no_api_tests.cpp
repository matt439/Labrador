#include <doctest/doctest.h>

#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/scripted_gamepad_source.h"

#include <emscripten/em_js.h>

using namespace labrador;

EM_JS(void, scripted_test_install_pad_trap, (), {
	globalThis.labradorScriptPadTrap = {
		saved: Object.getOwnPropertyDescriptor(globalThis, "navigator"),
		calls: 0,
	};
	Object.defineProperty(globalThis, "navigator", {
		configurable: true,
		value: {
			getGamepads() {
				globalThis.labradorScriptPadTrap.calls++;
				throw new Error("scripted input reached the physical pad API");
			},
		},
	});
});

EM_JS(int, scripted_test_remove_pad_trap, (), {
	const trap = globalThis.labradorScriptPadTrap;
	if (trap.saved) {
		Object.defineProperty(globalThis, "navigator", trap.saved);
	} else {
		delete globalThis.navigator;
	}
	delete globalThis.labradorScriptPadTrap;
	return trap.calls;
});

TEST_CASE("scripted web reader never consults the physical pad API")
{
	struct PadApiTrap
	{
		PadApiTrap() { scripted_test_install_pad_trap(); }
		~PadApiTrap() { CHECK(scripted_test_remove_pad_trap() == 0); }
	};
	PadApiTrap trap;
	{
		ScriptedGamepadSource source;
		GamepadReader reader(source);
		Gamepads pads(&reader);
		GamepadState connected;
		connected.connected = true;
		source.set_state(0, connected);
		pads.poll();
		CHECK(pads.vibrate(0, Vibration{ 0.5f, 0.5f, 0.5f }) ==
			VibrationResult::submitted);
		reader.suspend();
		reader.sample();
		reader.resume();
		pads.poll();
		CHECK(pads.vibrate(0, Vibration{ 0.5f, 0.5f, 0.5f }) ==
			VibrationResult::submitted);
		source.set_time(1.0);
		reader.sample();
		pads.stop_vibration(0);
		CHECK(pads.vibrate(0, Vibration{ 0.5f, 0.5f, 0.5f }) ==
			VibrationResult::submitted);
	}
}

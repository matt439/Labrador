#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/scripted_gamepad_source.h"

#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition)
	{
		if (!condition)
		{
			throw std::runtime_error("scripted pad consumer check failed");
		}
	}
}

int main()
{
	try
	{
		// The consumer links its ordinary engine once. Constructing a script
		// beside the shipping reader must not construct a second platform API.
		labrador::GamepadReader shipping;
		labrador::ScriptedGamepadSource source;
		{
			labrador::GamepadReader reader(source);
			labrador::Gamepads pads(&reader);
			labrador::GamepadState state;
			state.connected = true;
			source.set_state(0, state);
			pads.poll();
			require(pads.just_connected(0));
			state.buttons = labrador::gamepad_button_bit(labrador::GamepadButton::a);
			source.set_state(0, state);
			pads.poll();
			require(pads.pressed(0, labrador::GamepadButton::a));
			require(pads.vibrate(0, { 0.25f, 0.75f, 0.5f }) ==
				labrador::VibrationResult::submitted);
			reader.suspend();
			pads.poll();
			require(!pads.connected(0));
			reader.resume();
			pads.poll();
			require(pads.just_connected(0));
			require(!pads.pressed(0, labrador::GamepadButton::a));
			require(pads.vibrate(0, { 0.25f, 0.75f, 0.5f }) ==
				labrador::VibrationResult::submitted);
		}
		int stops = 0;
		for (const labrador::ScriptedGamepadRecord& record : source.records())
		{
			stops += record.event == labrador::ScriptedGamepadEvent::stop ? 1 : 0;
		}
		require(stops == 2 && !source.records_overflowed());
		std::cout << "PASS same-library scripted pad consumer\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}

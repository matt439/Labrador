#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/vibration_output.h"
#include "engine/input/web/standard_gamepad.h"

#include <emscripten/em_js.h>
#include <emscripten/html5.h>

#include <memory>
#include <vector>

// Global rather than in labrador::, because EM_JS declares a C function, and
// prefixed for the same reason.
//
// The Gamepad API's one portable rumble is the "dual-rumble" effect on
// Gamepad.vibrationActuator. A browser without it, or a pad whose actuator
// lists effects without that one, is unavailable output. The effect's own
// promise is caught: a rejection is a pad that would not rumble, which the
// engine has already answered as best it can, not an error for the page.
EM_JS(int, labrador_play_dual_rumble, (int index, double strong, double weak,
	double milliseconds), {
	if (typeof navigator === "undefined" || !navigator.getGamepads) {
		return 0;
	}
	const pad = navigator.getGamepads()[index];
	const actuator = pad ? pad.vibrationActuator : null;
	if (!actuator || typeof actuator.playEffect !== "function") {
		return 0;
	}
	if (Array.isArray(actuator.effects) &&
		!actuator.effects.includes("dual-rumble")) {
		return 0;
	}
	try {
		actuator.playEffect("dual-rumble", {
			startDelay: 0,
			duration: milliseconds,
			strongMagnitude: strong,
			weakMagnitude: weak,
		}).catch(() => {});
	} catch (error) {
		return 0;
	}
	return 1;
});

EM_JS(void, labrador_reset_rumble, (int index), {
	if (typeof navigator === "undefined" || !navigator.getGamepads) {
		return;
	}
	const pad = navigator.getGamepads()[index];
	const actuator = pad ? pad.vibrationActuator : null;
	if (!actuator || typeof actuator.reset !== "function") {
		return;
	}
	try {
		actuator.reset().catch(() => {});
	} catch (error) {
	}
});

namespace labrador
{
	namespace
	{
		// A slot's request goes to whichever browser pad is in it. The effect
		// carries its own duration, and VibrationOutput resets the actuator
		// at the same end, so a request ends at one time on both sides.
		struct WebVibration
		{
			const GamepadSlots* slots = nullptr;

			bool send(int slot, const Vibration& vibration)
			{
				const int index = (*this->slots)[slot];
				return index != no_gamepad && labrador_play_dual_rumble(index,
					vibration.low_frequency, vibration.high_frequency,
					static_cast<double>(vibration.seconds) * 1000.0) != 0;
			}

			void stop(int slot)
			{
				const int index = (*this->slots)[slot];
				if (index != no_gamepad)
				{
					labrador_reset_rumble(index);
				}
			}
		};
	}

	// The browser Gamepad API, which is polled: navigator.getGamepads() is a
	// snapshot taken on demand, which is the shape sample() already asks for,
	// so this backend needs no event queue and no thread. What a pad means is
	// standard_gamepad.h's, and this file is the part that asks the browser.
	//
	// SUSPENSION IS A FLAG HERE, because the API has none. Application
	// suspends the reader while the page is hidden, which is what the
	// background is to a page, and not when the canvas loses the keyboard:
	// a player on a pad has not left because they clicked the page beside the
	// game. A hidden page gets no frames to read in anyway; the flag is what
	// keeps a sample taken in one from handing back what it saw.
	struct GamepadReader::Impl
	{
		bool suspended = false;

		GamepadSlots slots = { no_gamepad, no_gamepad, no_gamepad, no_gamepad };
		GamepadState states[Gamepads::max_count];

		// Kept rather than rebuilt, so a sample allocates nothing once it has
		// seen as many pads as it ever will.
		std::vector<int> present;

		// After the slots it reads, so it is destroyed first and its last
		// stop still knows which pad each slot holds.
		VibrationOutput<WebVibration> vibration{ WebVibration{ &this->slots } };
	};

	GamepadReader::GamepadReader() :
		impl_(std::make_unique<Impl>())
	{
	}

	GamepadReader::~GamepadReader() = default;
	GamepadReader::GamepadReader(GamepadReader&&) noexcept = default;
	GamepadReader& GamepadReader::operator=(GamepadReader&&) noexcept = default;

	void GamepadReader::sample()
	{
		Impl& impl = *this->impl_;
		for (GamepadState& state : impl.states)
		{
			state = GamepadState();
		}

		// The slots outlast a suspension: a pad that is still there when the
		// page comes back is still the pad it was.
		if (impl.suspended)
		{
			return;
		}

		// Fails where there is no Gamepad API at all - Node, which runs the
		// tests, and a browser that withholds it - and every slot is then
		// empty, which is the truth.
		impl.present.clear();
		EmscriptenGamepadEvent pad;
		if (emscripten_sample_gamepad_data() == EMSCRIPTEN_RESULT_SUCCESS)
		{
			const int count = emscripten_get_num_gamepads();
			for (int index = 0; index < count; ++index)
			{
				// NO_DATA is a hole the browser left where a pad was.
				if (emscripten_get_gamepad_status(index, &pad) ==
					EMSCRIPTEN_RESULT_SUCCESS && pad.connected &&
					is_standard_gamepad(pad))
				{
					impl.present.push_back(index);
				}
			}
		}

		// The new assignment beside the old one, so a request on a slot whose
		// pad has gone, or been replaced, is stopped on the pad it was sent
		// to rather than on whichever arrived.
		GamepadSlots next = impl.slots;
		assign_gamepad_slots(next, impl.present);
		const double now = vibration_clock_seconds();
		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			impl.vibration.sampled(slot,
				next[slot] != no_gamepad && next[slot] == impl.slots[slot], now);
		}
		impl.slots = next;

		for (int slot = 0; slot < Gamepads::max_count; ++slot)
		{
			const int index = impl.slots[slot];
			if (index != no_gamepad &&
				emscripten_get_gamepad_status(index, &pad) ==
				EMSCRIPTEN_RESULT_SUCCESS)
			{
				impl.states[slot] = gamepad_state_from_standard(pad);
			}
		}
	}

	GamepadState GamepadReader::read(int slot) const
	{
		if (slot < 0 || slot >= Gamepads::max_count)
		{
			return GamepadState();
		}
		return this->impl_->states[slot];
	}

	void GamepadReader::suspend()
	{
		this->impl_->vibration.suspend();
		this->impl_->suspended = true;
	}

	void GamepadReader::resume()
	{
		this->impl_->suspended = false;
		this->impl_->vibration.resume();
	}

	bool GamepadReader::vibrate(int slot, const Vibration& vibration)
	{
		return this->impl_->vibration.start(slot, vibration,
			vibration_clock_seconds());
	}

	void GamepadReader::stop_vibration(int slot)
	{
		this->impl_->vibration.stop(slot);
	}
}

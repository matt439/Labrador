#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/input/web/standard_gamepad.h"

#include <emscripten/html5.h>

#include <memory>
#include <vector>

namespace labrador
{
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

		assign_gamepad_slots(impl.slots, impl.present);

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
		this->impl_->suspended = true;
	}

	void GamepadReader::resume()
	{
		this->impl_->suspended = false;
	}
}

#include "engine/input/gamepad_reader.h"
#include "engine/input/gamepads.h"
#include "engine/math/vector2f.h"

#include <emscripten/html5.h>

#include <cstring>
#include <memory>

using namespace mattmath;

namespace labrador
{
	// The browser Gamepad API, which is polled: navigator.getGamepads() is a
	// snapshot taken on demand, which is the shape read(slot) already asks
	// for, so this backend needs no event queue and no thread.
	//
	// SUSPENSION IS A FLAG HERE, because the API has none. A page in the
	// background is still handed pad state, so this backend does what XInput
	// is asked to do and answers disconnected until resumed.
	struct GamepadReader::Impl
	{
		bool suspended = false;
	};

	namespace
	{
		// The W3C "standard" layout, by index. Buttons 6 and 7 are the
		// triggers, read as analogue values below; 16 is the guide button,
		// which this engine has no name for.
		struct ButtonIndex
		{
			int index;
			GamepadButton button;
		};

		constexpr ButtonIndex standard_buttons[] = {
			{ 0, GamepadButton::a },
			{ 1, GamepadButton::b },
			{ 2, GamepadButton::x },
			{ 3, GamepadButton::y },
			{ 4, GamepadButton::left_shoulder },
			{ 5, GamepadButton::right_shoulder },
			{ 8, GamepadButton::back },
			{ 9, GamepadButton::start },
			{ 10, GamepadButton::left_stick },
			{ 11, GamepadButton::right_stick },
			{ 12, GamepadButton::dpad_up },
			{ 13, GamepadButton::dpad_down },
			{ 14, GamepadButton::dpad_left },
			{ 15, GamepadButton::dpad_right },
		};

		constexpr int standard_button_count = 17;
		constexpr int standard_axis_count = 4;
		constexpr int left_trigger_index = 6;
		constexpr int right_trigger_index = 7;
	}

	GamepadReader::GamepadReader() :
		impl_(std::make_unique<Impl>())
	{
	}

	GamepadReader::~GamepadReader() = default;
	GamepadReader::GamepadReader(GamepadReader&&) noexcept = default;
	GamepadReader& GamepadReader::operator=(GamepadReader&&) noexcept = default;

	GamepadState GamepadReader::read(int slot) const
	{
		GamepadState result;
		if (this->impl_->suspended || slot < 0 || slot >= Gamepads::max_count)
		{
			return result;
		}

		// Fails where there is no Gamepad API at all - Node, which runs the
		// tests, and a browser that withholds it - and every slot is then
		// empty, which is the truth.
		if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
		{
			return result;
		}

		EmscriptenGamepadEvent pad;
		if (emscripten_get_gamepad_status(slot, &pad) !=
			EMSCRIPTEN_RESULT_SUCCESS || !pad.connected)
		{
			return result;
		}

		// A pad the browser could not map onto the standard layout reports
		// its buttons and axes in whatever order its driver chose, so reading
		// it by index would put "a" wherever that happened to be. It is
		// answered as absent rather than guessed at.
		if (std::strcmp(pad.mapping, "standard") != 0 ||
			pad.numButtons < standard_button_count ||
			pad.numAxes < standard_axis_count)
		{
			return result;
		}

		result.connected = true;

		// No negation: the standard layout is already +y down, which is this
		// engine's convention, where XInput's is +y up.
		result.left_stick = Vector2F(static_cast<float>(pad.axis[0]),
			static_cast<float>(pad.axis[1]));
		result.right_stick = Vector2F(static_cast<float>(pad.axis[2]),
			static_cast<float>(pad.axis[3]));

		result.left_trigger =
			static_cast<float>(pad.analogButton[left_trigger_index]);
		result.right_trigger =
			static_cast<float>(pad.analogButton[right_trigger_index]);

		uint16_t buttons = 0;
		for (const ButtonIndex& entry : standard_buttons)
		{
			if (pad.digitalButton[entry.index])
			{
				buttons = static_cast<uint16_t>(buttons |
					(1u << static_cast<unsigned int>(entry.button)));
			}
		}
		result.buttons = buttons;

		return result;
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

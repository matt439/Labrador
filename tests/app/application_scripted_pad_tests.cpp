#include <doctest/doctest.h>

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/input/scripted_gamepad_source.h"

#include <Windows.h>

#include <memory>
#include <tuple>

using namespace labrador;

namespace
{
	GamepadState pad_with(GamepadButton button)
	{
		GamepadState state;
		state.connected = true;
		state.buttons = gamepad_button_bit(button);
		return state;
	}

	class PadState : public State
	{
	public:
		PadState(Application* app, ScriptedGamepadSource* source, int* steps) :
			app_(app), source_(source), steps_(steps)
		{
		}

		void init() override {}
		void draw(Renderer& /*renderer*/) const override {}

		void update(float /*dt*/) override
		{
			Gamepads& pads = *this->app_->gamepads();
			const HWND window = static_cast<HWND>(this->app_->window());
			++*this->steps_;
			switch (*this->steps_)
			{
			case 1:
				CHECK(pads.just_connected(0));
				CHECK_FALSE(pads.pressed(0, GamepadButton::a));
				this->source_->set_state(0, pad_with(GamepadButton::b));
				break;
			case 2:
				CHECK(pads.pressed(0, GamepadButton::b));
				CHECK(pads.vibrate(0, Vibration{ 0.5f, 0.25f, 0.5f }) ==
					VibrationResult::submitted);
				this->source_->set_time(0.5);
				break;
			case 3:
				REQUIRE_FALSE(this->source_->records().empty());
				CHECK(this->source_->records().back().event == ScriptedGamepadEvent::stop);
				CHECK(pads.vibrate(0, Vibration{ 0.5f, 0.25f, 0.5f }) ==
					VibrationResult::submitted);
				std::ignore = SendMessageW(window, WM_ACTIVATEAPP, FALSE, 0);
				this->source_->set_state(0, pad_with(GamepadButton::a));
				break;
			case 4:
				CHECK_FALSE(pads.connected(0));
				std::ignore = SendMessageW(window, WM_ACTIVATEAPP, TRUE, 0);
				break;
			case 5:
				CHECK(pads.just_connected(0));
				CHECK_FALSE(pads.pressed(0, GamepadButton::a));
				this->source_->set_state(0, GamepadState());
				break;
			case 6:
				CHECK(pads.just_disconnected(0));
				this->source_->set_state(0, pad_with(GamepadButton::b));
				break;
			case 7:
				CHECK(pads.just_connected(0));
				CHECK_FALSE(pads.pressed(0, GamepadButton::b));
				CHECK(pads.vibrate(0, Vibration{ 0.25f, 0.5f, 0.5f }) ==
					VibrationResult::submitted);
				this->app_->quit();
				break;
			default:
				break;
			}
		}

	private:
		Application* app_;
		ScriptedGamepadSource* source_;
		int* steps_;
	};
}

TEST_CASE("Application uses its scripted pads through real loop activation and teardown")
{
	ScriptedGamepadSource source;
	source.set_state(0, pad_with(GamepadButton::a));
	int steps = 0;
	{
		ApplicationOptions options;
		options.scripted_gamepad_source = &source;
		options.window_class_name = L"ScriptedPadApplicationTest";
		options.visible = false;
		options.max_threads = 1;
		options.view_capacity = 1;
		options.target_fps = 1000;
		Application app(options);
		app.initialize();
		app.run(std::make_unique<PadState>(&app, &source, &steps));
	}
	CHECK(steps >= 7);
	int stops = 0;
	int suspensions = 0;
	int resumptions = 0;
	for (const ScriptedGamepadRecord& record : source.records())
	{
		stops += record.event == ScriptedGamepadEvent::stop ? 1 : 0;
		suspensions += record.event == ScriptedGamepadEvent::suspend ? 1 : 0;
		resumptions += record.event == ScriptedGamepadEvent::resume ? 1 : 0;
	}
	CHECK(stops == 3);
	CHECK(suspensions >= 1);
	CHECK(resumptions >= 1);
	CHECK_FALSE(source.records_overflowed());
	// Teardown releases the borrow and stops the final active request.
	CHECK_NOTHROW(static_cast<void>(GamepadReader(source)));
}

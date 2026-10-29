#include "samples/linesweeper/states/play_state.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/states/pause_state.h"
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
#include "samples/linesweeper/web/test_input.h"
#endif

#include <memory>
#include <optional>
#include <string>

using namespace labrador;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// Spelt once, resolved to a handle once, never looked up per frame
		// (PHILOSOPHY T7, T8).
		const std::string font_name = "jetbrains_mono_bold_16";

		// The one white texel every block, panel and spark on this screen is
		// drawn from. Spelt here as well as in board_view.cpp for the reason
		// font_name is: the object that draws it resolves its own handle,
		// except the particle field, which takes a resolved one so that it can
		// be stepped with no device at all (particles.h).
		const std::string block_texture_name = "white";

		// One line per device, under the board in landscape. The capture tool
		// and the frame bench copy these two strings and have to follow them.
		const wchar_t* const hint_text =
			L"keyboard   arrows move   Z X rotate   space drops   C holds";
		const wchar_t* const pad_hint_text =
			L"pad        d-pad moves   A B rotate   Y drops       X holds";

		// Game-owned bindings (T1). Key::z names the physical position a US
		// layout calls Z, independent of the character another layout prints.
		struct Binding
		{
			Key key;
			std::uint8_t button;
		};

		constexpr Binding bindings[] = {
			{ Key::left, button_left },
			{ Key::right, button_right },
			{ Key::down, button_soft_drop },
			{ Key::space, button_hard_drop },
			{ Key::up, button_rotate_clockwise },
			{ Key::x, button_rotate_clockwise },
			{ Key::z, button_rotate_anticlockwise },
			{ Key::c, button_hold },
			{ Key::shift, button_hold },
		};

		// Separate device tables allow several bindings per action without a
		// sentinel for missing keys or pad buttons. Gameplay reads the d-pad:
		// tick() takes held button bits and owns their repeat timing. The menu
		// separately accepts the stick through pad_direction().
		struct PadBinding
		{
			GamepadButton pad;
			std::uint8_t button;
		};

		constexpr PadBinding pad_bindings[] = {
			{ GamepadButton::dpad_left, button_left },
			{ GamepadButton::dpad_right, button_right },
			{ GamepadButton::dpad_down, button_soft_drop },
			{ GamepadButton::y, button_hard_drop },
			{ GamepadButton::dpad_up, button_rotate_clockwise },
			{ GamepadButton::a, button_rotate_clockwise },
			{ GamepadButton::b, button_rotate_anticlockwise },
			{ GamepadButton::x, button_hold },
			{ GamepadButton::left_shoulder, button_hold },
		};

		// Translate presentation's named touch controls into rules bits here.
		// Pause has no rules bit: update() handles its press as state policy.
		struct ControlBinding
		{
			Control control;
			std::uint8_t button;
		};

		constexpr ControlBinding control_bindings[] = {
			{ Control::left, button_left },
			{ Control::right, button_right },
			{ Control::soft_drop, button_soft_drop },
			{ Control::hard_drop, button_hard_drop },
			{ Control::rotate_clockwise, button_rotate_clockwise },
			{ Control::rotate_anticlockwise, button_rotate_anticlockwise },
			{ Control::hold, button_hold },
		};
	}

	PlayState::PlayState(Application* app, Motion motion,
		void (*report_palette)(Palette)) :
		app_(app),
		motion_(motion),
		report_palette_(report_palette)
	{
	}

	void PlayState::init()
	{
		RenderResources* resources = this->app_->render_resources();

		// One view needs neither a draw worker pool nor a partitioner.
		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

		// The board reads the World this state owns. It never writes one, and
		// it could not: presentation/ does not include tick.h.
		this->board_ = this->scene_->add(std::make_unique<BoardView>(
			&this->world_, resources, &this->palette_));

		// Register after the board so sparks draw over its cells. The field
		// compares successive Worlds to find events and reads last_tick_ for
		// the locked piece that compaction can hide. StepPosition supports
		// const extrapolation between updates, including suspension detection.
		this->particles_ = this->scene_->add(std::make_unique<ParticleField>(
			&this->world_, &this->last_tick_,
			resources->resolve_texture(block_texture_name),
			&this->app_->step_position(), this->motion_, &this->palette_));

		// The top-out message must draw over the burst on its first frame.
		this->banner_ = this->scene_->add(
			std::make_unique<TopOutBanner>(&this->world_, resources));

		// One line per device, left-aligned on the HUD column at x = 344 and
		// columnised against each other by hand, which only works because the
		// font is monospaced.
		//
		// At the bottom of the landscape screen, which is board space rather
		// than the buffer: the camera puts it where the buffer's bottom is
		// (arrangement.h).
		this->hint_ = this->scene_->add(std::make_unique<Label>(
			hint_text, font_name, Vector2F(344.0f, landscape_height - 60.0f),
			resources,
			Colour::dark_gray));

		this->pad_hint_ = this->scene_->add(std::make_unique<Label>(
			pad_hint_text, font_name,
			Vector2F(344.0f, landscape_height - 36.0f), resources,
			Colour::dark_gray));

		// Touch controls draw over the other objects once a finger is seen.
		this->controls_ = this->scene_->add(
			std::make_unique<TouchControls>(resources));

		// Commit pending objects before any draw, including one before update().
		this->scene_->end_tick();

		this->arrange_screen();
		this->show_hints();

		this->reported_palette_ = this->palette_;
		if (this->report_palette_ != nullptr)
		{
			this->report_palette_(this->palette_);
		}

		// Loading may finish after the page was hidden. That state missed the
		// deactivation edge, so it reads the context's retained fact instead.
		if (!this->context()->active())
		{
			this->on_deactivated();
		}
	}

	void PlayState::show_hints()
	{
		// Hide key hints after the first touch and in portrait, where these
		// landscape lines extend outside the framed board.
		const bool hints = !this->touch_seen_ &&
			this->arrangement_.orientation == Orientation::landscape;

		if (hints != this->hints_shown_)
		{
			this->hints_shown_ = hints;
			this->hint_->set_text(hints ? hint_text : L"");
			this->pad_hint_->set_text(hints ? pad_hint_text : L"");
		}
	}

	void PlayState::arrange_screen()
	{
		this->arrangement_ =
			arrange(this->app_->resolution_manager()->resolution_vec());

		this->scene_->clear_views();
		this->scene_->add_view(this->arrangement_.viewport,
			this->arrangement_.camera);
	}

	// Read held controls: tick() derives edges by comparing this byte with
	// World's previous_input, so a recording includes its own press history.
	// A key pressed and released between polls is absent from the sampled byte.
	std::uint8_t PlayState::read_input() const
	{
		const Keyboard& keyboard = *this->app_->keyboard();
		const Gamepads& pads = *this->app_->gamepads();

		std::uint8_t input = button_none;

		for (const Binding& binding : bindings)
		{
			if (keyboard.held(binding.key))
			{
				input |= binding.button;
			}
		}

		// OR sources into one byte: the rules do not distinguish devices.
		// This one-player sample reads slot zero; an empty slot contributes
		// neutral input without a separate connected() test.
		for (const PadBinding& binding : pad_bindings)
		{
			if (pads.held(0, binding.pad))
			{
				input |= binding.button;
			}
		}

		// Every finger, against the rectangles the controls were drawn at
		// this tick. Held, like the other two, so a thumb resting on LEFT is
		// the same delay and the same repeat as a held arrow key.
		const ControlSet touched = controls_held(this->arrangement_,
			this->app_->touch()->points());

		for (const ControlBinding& binding : control_bindings)
		{
			if ((touched & control_bit(binding.control)) != 0)
			{
				input |= binding.button;
			}
		}

		return input;
	}

	// Pause suspends the match rather than stepping it, so neither the menu nor
	// its typed result belongs to the byte recorded for tick().
	void PlayState::open_pause_menu()
	{
		this->context()->push<PauseChoice>(
			std::make_unique<PauseState>(this->app_, &this->palette_),
			[this](const PauseChoice& choice)
			{
				// Before QUIT acts, so a player who changed palette and then
				// quit is still reported.
				if (this->palette_ != this->reported_palette_)
				{
					this->reported_palette_ = this->palette_;
					if (this->report_palette_ != nullptr)
					{
						this->report_palette_(this->palette_);
					}
				}

				switch (choice)
				{
				case PauseChoice::restart:
					// Assignment preserves all presentation loans into world_.
					this->world_ = World{};
					break;

				case PauseChoice::quit:
					this->app_->quit();
					break;

				case PauseChoice::resume:
				default:
					break;
				}
			});
	}

	void PlayState::on_resume()
	{
		this->covered_ = false;
		// The buffer may have changed shape under the menu, and the gate
		// below reads the touch controls where they are now.
		this->arrange_screen();

		// StateContext invokes this before the result callback, so the same
		// held-input gate also protects a World replaced by RESTART.
		this->gate_.close_over(this->read_input());
	}

	void PlayState::on_suspend()
	{
		this->covered_ = true;
	}

	void PlayState::on_deactivated()
	{
		// A hidden browser tab need not get another update before it returns.
		// Queue the pause on this edge, rather than waiting for an input poll.
		if (!this->covered_ && this->world_.topped_out == 0)
		{
			this->open_pause_menu();
		}
	}

	void PlayState::update(float dt)
	{
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
		TestInputStep test_step(*this->app_->keyboard(), "play", this,
			[](const void* source)
			{
				const PlayState& state = *static_cast<const PlayState*>(source);
				return TestInputSnapshot{ -1, static_cast<int>(state.palette_) };
			});
		if (!test_step.allowed()) return;
#endif
		this->arrange_screen();

		const Touch& touch = *this->app_->touch();

		// Remember the first observed finger for this state's lifetime.
		if (!this->touch_seen_ && !touch.points().empty())
		{
			this->touch_seen_ = true;
			this->banner_->set_touch(true);
		}

		this->show_hints();

		// The taps that started this frame: pause and restart are edges, as
		// Escape and R are, so they are read from the points that went down
		// now rather than from what is held.
		bool pause_tapped = false;
		bool tapped_off_controls = false;

		for (const TouchPoint& point : touch.points())
		{
			if (!touch.pressed(point.id))
			{
				continue;
			}

			const std::optional<Control> control =
				control_at(this->arrangement_, point.position);

			if (!control.has_value())
			{
				tapped_off_controls = true;
			}
			else if (*control == Control::pause)
			{
				pause_tapped = true;
			}
		}

		// Before the restart test and before tick(), so the frame a player
		// pauses on is not also a frame the match steps.
		//
		// Not while topped out: the match is already over, the banner says what
		// to press, and a pause menu offering RESUME over a finished match is
		// offering something that does not exist.
		if (this->world_.topped_out == 0 &&
			(this->app_->keyboard()->just_unfocused() ||
				this->app_->keyboard()->pressed(Key::escape) ||
				this->app_->keyboard()->pressed(Key::p) ||
				this->app_->gamepads()->pressed(0, GamepadButton::start) ||
				pause_tapped))
		{
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
			test_step.result("pause");
#endif
			this->open_pause_menu();
			return;
		}

		// Restart is an edge outside the rules byte and assigns World{} in
		// place. A touch must begin off the controls: repeated DROP taps must
		// not also restart a match as it tops out.
		if (this->world_.topped_out != 0 &&
			(this->app_->keyboard()->pressed(Key::r) ||
				this->app_->gamepads()->pressed(0, GamepadButton::start) ||
				tapped_off_controls))
		{
			this->world_ = World{};
		}

		// One rules tick per fixed update; Application may run several updates
		// before a rendered frame. Rules durations are tick counts, so the
		// entry point's 60 Hz remains part of this game's timing. dt advances
		// presentation only. The gate masks held input until released after
		// resume; the resulting byte is sufficient for deterministic replay.
		this->last_tick_ = tick(this->world_, this->gate_.pass(this->read_input()));

		this->controls_->set(this->arrangement_,
			controls_held(this->arrangement_, touch.points()),
			this->touch_seen_);

		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void PlayState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer);
	}
}

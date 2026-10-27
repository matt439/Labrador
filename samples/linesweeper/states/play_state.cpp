#include "samples/linesweeper/states/play_state.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/states/pause_state.h"

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

		// The bindings, and the whole of the input layer.
		//
		// A table of pairs rather than a chain of ifs, because it is data and
		// reads as data - and it is in this file because bindings and any
		// rebinding are the game's (PHILOSOPHY, The boundary; T1), not the
		// engine's. Key positions, not characters: Key::z is
		// the key a US layout calls Z whatever a French one prints on it,
		// which is what a movement binding wants (keyboard.h).
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

		// The pad's half, and TWO TABLES RATHER THAN ONE ROW PER ACTION.
		//
		// The obvious shape is a single row naming both devices, and it does
		// not fit: `up` and `x` both rotate clockwise and `c` and `shift` both
		// hold, so a combined row would need a "no button on this device"
		// enumerator - and GamepadButton has none, deliberately (gamepad.h).
		// Adding one to an engine header so a sample's table lines up is the
		// tail wagging the dog. Two tables cost a loop each, ask the engine
		// for nothing, and a third device would be a third table rather than
		// an edit to every row above.
		//
		// The d-pad and not the stick, because tick() takes a byte of held
		// buttons: an axis would need a threshold here to become one, on top
		// of the repeat rate the rules already own (tables.h).
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

		// The glass's half: the third table, and the third device the pad's
		// pricing said would cost a table and a loop rather than an edit to
		// the first two (README, Still open). The controls are named for what
		// they are on the screen because presentation/ cannot see these bits
		// (touch_controls.h), and pause has no row because it is not an input
		// to tick() - update() reads it as an edge, as it reads Escape.
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

	PlayState::PlayState(Application* app, Motion motion) :
		app_(app),
		motion_(motion)
	{
	}

	void PlayState::init()
	{
		RenderResources* resources = this->app_->render_resources();

		// One view, so no thread pool and no partitioner - the scene's fan-out
		// is over views, and a fan-out over one is a fan-out over nothing.
		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

		// The board reads the World this state owns. It never writes one, and
		// it could not: presentation/ does not include tick.h.
		this->board_ = this->scene_->add(
			std::make_unique<BoardView>(&this->world_, resources));

		// AFTER THE BOARD, AND THAT IS THE WHOLE DEPTH SYSTEM. Every draw in
		// this sample is at layer_depth 0, so the order the scene was filled
		// in is the order the quads are submitted in and sparks are drawn over
		// the stack they came out of.
		//
		// It is handed the same const World* the board holds, and a pointer
		// to what the latest tick() returned. The state never tells it a line
		// was cleared: it works that out by keeping last frame's match and
		// comparing, which is a thing only a 276-byte trivially copyable
		// value makes cheap enough to do every frame - and the second pointer
		// is the one fact that comparison cannot recover, the piece as it
		// locked (particles.h). The third is the shell's step, which carries
		// the sparks on between steps on a display faster than one.
		this->particles_ = this->scene_->add(std::make_unique<ParticleField>(
			&this->world_, &this->last_tick_,
			resources->resolve_texture(block_texture_name),
			&this->app_->step_position(), this->motion_));

		// LAST, AND OVER THE SPARKS. The top-out banner appears on the exact
		// frame the field throws its largest burst, so the one screen a player
		// has to read is the one the effect would bury.
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

		// LAST, over everything, because a button under a spark is a button
		// a thumb cannot find. Hidden until a finger touches the screen.
		this->controls_ = this->scene_->add(
			std::make_unique<TouchControls>(resources));

		// Adds are pending until a tick ends, and the frame drawn before the
		// first update() is a real frame - so whoever fills a scene ends its
		// first tick by hand.
		this->scene_->end_tick();

		this->arrange_screen();
		this->show_hints();
	}

	void PlayState::show_hints()
	{
		// The hints name keys, so they go when the screen has none - and in
		// portrait, where the line is wider than the board block the camera
		// frames and would run off the side.
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

	// held(), NEVER pressed(), and the choice is not arbitrary.
	//
	// The engine's Keyboard computes edges of its own - down now, up last
	// frame - and using them here would work exactly once. It would also move
	// the edge out of the recording: tick() derives every press from `input`
	// against `previous_input`, so a replay carries its own edges and a
	// recorded hard drop stays one hard drop. Reading edges here would make
	// the byte handed to the simulation depend on which frames the window had
	// the keyboard, and a match would stop being a function of its inputs.
	//
	// The engine's rule that a key pressed and released between two polls is
	// not seen at all (keyboard.h) still holds, and is right: at sixty ticks a
	// second no human produces one.
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

		// OR, AND NO connected() AROUND IT. The byte tick() gets does not say
		// which device set a bit and must not - a player holding left on the
		// d-pad while tapping Z on the keyboard is one input, and the
		// recording of it is one byte either way. An empty slot reads as a
		// neutral state rather than a stale one (gamepad.h), so this loop
		// contributes nothing when there is no pad and needs no guard to.
		//
		// Slot 0, spelt here rather than tracked, because this is a
		// one-player game. Which pad is which player is a decision the engine
		// deliberately leaves to a game (gamepads.h), and this is the whole of
		// this game's.
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

	// PAUSING IS NOT AN INPUT TO tick(), which is the same distinction the
	// restart below turns on. It suspends the match rather than stepping it, so
	// it reads an edge and falls outside anything a recording would carry.
	//
	// Escape and Start, and not P alone: P is a letter the pause screen also
	// accepts as a way out, and a player who opened the menu with a key expects
	// that key to close it.
	void PlayState::open_pause_menu()
	{
		this->context()->push<PauseChoice>(
			std::make_unique<PauseState>(this->app_),
			[this](const PauseChoice& choice)
			{
				switch (choice)
				{
				case PauseChoice::restart:
					// The same one line the top-out restart is, reached from a
					// menu instead of from a key. There is no reset() here
					// either - README, The match is one value.
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
		// The buffer may have changed shape under the menu, and the gate
		// below reads the touch controls where they are now.
		this->arrange_screen();

		// Before on_result runs, which is the order state_context.h fixes -
		// so this covers RESTART's fresh World as well as RESUME's.
		this->gate_.close_over(this->read_input());
	}

	void PlayState::update(float dt)
	{
		this->arrange_screen();

		const Touch& touch = *this->app_->touch();

		// THE FIRST FINGER SHOWS THE CONTROLS, and nothing before it does: a
		// player at a keyboard never sees eight buttons over the screen.
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
			(this->app_->keyboard()->pressed(Key::escape) ||
				this->app_->keyboard()->pressed(Key::p) ||
				this->app_->gamepads()->pressed(0, GamepadButton::start) ||
				pause_tapped))
		{
			this->open_pause_menu();
			return;
		}

		// RESTARTING IS AN ASSIGNMENT, and this is the line README's "The
		// match is one value" is arguing for. There is no reset(), nothing to
		// notify, no scene to rebuild and no allocation - the board holds a
		// pointer to this member and the pointer stays good.
		//
		// pressed() here where read_input() above insists on held(), and the
		// difference is which side of the recording the edge falls on.
		// Restarting is not an input to tick() - it replaces the value tick()
		// runs on - so it is free to be an edge, and wants to be: a held R
		// would restart sixty times a second.
		//
		// On the glass it is a tap anywhere that is not a button, which is
		// what the banner says once a finger has been seen. Not a tap on one:
		// a player hammering DROP as the stack reaches the top would restart
		// the match they had not finished looking at.
		if (this->world_.topped_out != 0 &&
			(this->app_->keyboard()->pressed(Key::r) ||
				this->app_->gamepads()->pressed(0, GamepadButton::start) ||
				tapped_off_controls))
		{
			this->world_ = World{};
		}

		// ONE STEP, ONE FRAME, AND dt IS NOT PASSED IN. Every duration in the
		// rules is counted in fixed ticks, so the match is a function of its
		// starting value and the bytes handed to tick() - which is what makes
		// a recording replayable and what makes main.cpp's target_fps = 60
		// load-bearing rather than decorative. dt belongs to the presentation,
		// which is the half allowed to be smooth.
		//
		// A frame that took too long therefore runs the simulation slow rather
		// than dropping a piece through the floor, which is the trade every
		// fixed-step game makes and the right one for a game where a tick is
		// a rule.
		// Through the gate, which is a no-op on every frame but the ones
		// after a pause menu closed with a key the match also reads. What
		// comes back is kept for the field, which reads it on this frame's
		// update below.
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

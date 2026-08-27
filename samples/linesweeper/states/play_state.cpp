#include "samples/linesweeper/states/play_state.h"

#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/states/pause_state.h"

#include <memory>
#include <string>

using namespace mattmath;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// Spelt once, resolved to a handle once, never looked up per frame
		// (PHILOSOPHY T7, T8).
		const std::string font_name = "courier_new_bold_16";

		// The one white texel every block, panel and spark on this screen is
		// drawn from. Spelt here as well as in board_view.cpp for the reason
		// font_name is: the object that draws it resolves its own handle,
		// except the particle field, which takes a resolved one so that it can
		// be stepped with no device at all (particles.h).
		const std::string block_texture_name = "white";

		// The bindings, and the whole of the input layer.
		//
		// A table of pairs rather than a chain of ifs, because it is data and
		// reads as data - but it is a table in this file and not a rebinding
		// system, which is the speculative framework T1 rules out until a
		// second client wants one. Key positions, not characters: Key::z is
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
	}

	PlayState::PlayState(Application* app) : app_(app)
	{

	}

	void PlayState::init()
	{
		const Vector2F resolution =
			this->app_->resolution_manager()->resolution_vec();

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
		// It is handed the same const World* the board holds and nothing else.
		// The state never tells it a line was cleared: it works that out by
		// keeping last frame's match and comparing, which is a thing only a
		// 276-byte trivially copyable value makes cheap enough to do every
		// frame (particles.h).
		this->particles_ = this->scene_->add(std::make_unique<ParticleField>(
			&this->world_, resources->resolve_texture(block_texture_name)));

		// LAST, AND OVER THE SPARKS. The top-out banner appears on the exact
		// frame the field throws its largest burst, so the one screen a player
		// has to read is the one the effect would bury.
		this->banner_ = this->scene_->add(
			std::make_unique<TopOutBanner>(&this->world_, resources));

		// One line per device, left-aligned on the HUD column at x = 344 and
		// columnised against each other by hand, which only works because the
		// font is monospaced.
		this->hint_ = this->scene_->add(std::make_unique<Label>(
			L"keyboard   arrows move   Z X rotate   space drops   C holds",
			font_name, Vector2F(344.0f, resolution.y - 60.0f), resources,
			Colour::dark_gray));

		this->pad_hint_ = this->scene_->add(std::make_unique<Label>(
			L"pad        d-pad moves   A B rotate   Y drops       X holds",
			font_name, Vector2F(344.0f, resolution.y - 36.0f), resources,
			Colour::dark_gray));

		// Adds are pending until a tick ends, and the frame drawn before the
		// first update() is a real frame - so whoever fills a scene ends its
		// first tick by hand.
		this->scene_->end_tick();

		this->scene_->add_view(Viewport(RectangleF(Vector2F::ZERO, resolution)));
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

	void PlayState::update(float dt)
	{
		// Before the restart test and before tick(), so the frame a player
		// pauses on is not also a frame the match steps.
		//
		// Not while topped out: the match is already over, the banner says what
		// to press, and a pause menu offering RESUME over a finished match is
		// offering something that does not exist.
		if (this->world_.topped_out == 0 &&
			(this->app_->keyboard()->pressed(Key::escape) ||
				this->app_->keyboard()->pressed(Key::p) ||
				this->app_->gamepads()->pressed(0, GamepadButton::start)))
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
		if (this->world_.topped_out != 0 &&
			(this->app_->keyboard()->pressed(Key::r) ||
				this->app_->gamepads()->pressed(0, GamepadButton::start)))
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
		tick(this->world_, this->read_input());

		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void PlayState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer);
	}
}

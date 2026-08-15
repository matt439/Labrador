#include "samples/linesweeper/states/play_state.h"

#include "engine/input/keyboard.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "samples/linesweeper/rules/tick.h"

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

		this->hint_ = this->scene_->add(std::make_unique<Label>(
			L"arrows move   Z X rotate   space drops   C holds",
			font_name, Vector2F(344.0f, resolution.y - 40.0f), resources,
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

		std::uint8_t input = button_none;

		for (const Binding& binding : bindings)
		{
			if (keyboard.held(binding.key))
			{
				input |= binding.button;
			}
		}

		return input;
	}

	void PlayState::update(float dt)
	{
		// RESTARTING IS AN ASSIGNMENT, and this is the line README's "The
		// match is one value" is arguing for. There is no reset(), nothing to
		// notify, no scene to rebuild and no allocation - the board holds a
		// pointer to this member and the pointer stays good.
		if (this->world_.topped_out != 0 &&
			this->app_->keyboard()->pressed(Key::r))
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

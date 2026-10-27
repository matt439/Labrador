#include "samples/linesweeper/states/pause_state.h"

#include "engine/input/direction.h"
#include "engine/input/gamepads.h"
#include "engine/input/keyboard.h"
#include "engine/input/touch.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/viewport.h"
#include "samples/linesweeper/presentation/palette.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

using namespace labrador;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		const std::string font_name = "jetbrains_mono_bold_16";
		const std::string block_texture_name = "white";

		// The rows, in the order they are read rather than the order they are
		// navigated - `FocusGroup::add` says declaration order decides nothing
		// and `bounds()` decides everything, so this list is free to read the
		// way a player would say it.
		constexpr float title_y = 236.0f;
		constexpr float first_row_y = 312.0f;
		constexpr float row_spacing = 44.0f;

		constexpr Colour title_colour(200, 208, 222);

		// How much of the screen a row answers a finger over: the full gap
		// between rows, and this far either side of the middle. A row's own
		// bounds are the text the font measured, sixteen units tall, which is
		// a target a thumb misses on a phone.
		constexpr float row_tap_half_width = 160.0f;

		// How dark the match goes while the menu is up. Premultiplied black,
		// so it leaves a quarter of the board showing through and reads as a
		// pause rather than as a screen that replaced one (palette.h).
		constexpr float scrim_alpha = 0.75f;

		// The dimming quad, and the one thing on this screen that is not a
		// widget.
		//
		// It has no focus, no name, no colour a cursor changes and nothing to
		// navigate to, so making it a `UiWidget` would be claiming three
		// properties it has none of - and `nearest_in_direction` skips
		// zero-area boxes precisely so that things which are not destinations
		// stay out of the walk. A full-screen box is the opposite problem: it
		// would be a destination that swallows every press.
		//
		// It is not a `UiTexture` either, which is the leaf that would
		// otherwise fit. That one takes a sheet name and a frame name, and this
		// sample has no sprite sheet - one white texel and a font is the whole
		// of its content, which is the sentence the in-tree-samples argument
		// rests on (README). Fifteen lines here is cheaper than a sheet in the
		// manifest.
		//
		// It covers what the screen shows, which the state works out every
		// tick, so it borrows that rectangle rather than keeping a copy that a
		// resize would leave behind.
		class Scrim final : public GameObject
		{
		public:
			Scrim(TextureHandle block, const RectangleF* area) :
				block_(block),
				area_(area)
			{

			}

			void update(float /*dt*/) override
			{

			}

			void draw(DrawList& draw_list) const override
			{
				draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
					*this->area_, faded(Colour(0.0f, 0.0f, 0.0f), scrim_alpha),
					0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
			}

			RectangleF bounds() const override
			{
				return *this->area_;
			}

		private:
			TextureHandle block_;
			const RectangleF* area_ = nullptr;
		};
	}

	PauseState::PauseState(Application* app) : app_(app)
	{

	}

	void PauseState::init()
	{
		RenderResources* resources = this->app_->render_resources();
		const FontHandle font = resources->resolve_sprite_font(font_name);

		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

		// The match's arrangement, worked out the same way from the same
		// buffer, so the menu sits over the board whichever way up the screen
		// is (arrangement.h). Before the scrim, which borrows what it shows.
		this->arrange_screen();

		this->scene_->add(std::make_unique<Scrim>(
			resources->resolve_texture(block_texture_name),
			&this->arrangement_.visible));

		// CENTRED BY MEASURING, NOT BY COUNTING CHARACTERS. The HUD in
		// board_view.cpp columnises by hand and says it only works because the
		// font is monospaced; that is true and it is a property of this one
		// font. A menu whose rows are different lengths should not inherit it,
		// and `measure_text` is on the seam for exactly this
		// (render_resources.h). Measured once here, never in draw().
		//
		// Centred on the landscape screen, which in portrait is the middle of
		// the well: both arrangements put that column in the middle of the
		// buffer.
		//
		// AND PUT ON A WHOLE UNIT OF BOARD SPACE. Half of an odd width is half
		// a unit, and at 1280x720, where the arrangement's camera is the
		// identity (presentation/arrangement.h), a unit is a pixel:
		// point-sampled text there is a tie each backend breaks its own way
		// (renderer.h, draw_text). RESTART is one such row. At any other
		// buffer the camera scales the text, which is outside draw_text's
		// contract at a scale of one, and no rounding of the position puts
		// every glyph's texels on whole pixels.
		const auto centred = [&](const std::wstring& text, float y)
		{
			const Vector2F size = resources->measure_text(font, text);

			return Vector2F(std::floor((landscape_width - size.x) * 0.5f), y);
		};

		this->scene_->add(std::make_unique<UiText>("title", L"PAUSED",
			font_name, centred(L"PAUSED", title_y), resources, title_colour));

		// THE THREE ROWS, AND THE ACTIONS ARE THE GAME'S HALF OF button.h's
		// split: the engine knows which row the cursor is on and when the
		// player pressed A, and knows nothing about what pressing it does.
		//
		// Each action pops this state with its answer. That is safe from
		// inside `update()` - which is where `FocusGroup::activate` is called
		// from - because `StateContext::pop` is queued while the context has
		// something of its own on the stack, and applied after update()
		// returns. Without that it would be a use-after-free of the most
		// literal kind: the pop destroys this object, and the `std::function`
		// being executed is a member of a member of it.
		struct Row
		{
			const wchar_t* text;
			PauseChoice choice;
		};

		const Row rows[] = {
			{ L"RESUME", PauseChoice::resume },
			{ L"RESTART", PauseChoice::restart },
			{ L"QUIT", PauseChoice::quit },
		};

		for (int index = 0; index < 3; ++index)
		{
			const std::wstring text = rows[index].text;
			const PauseChoice choice = rows[index].choice;

			UiText* row = this->scene_->add(std::make_unique<UiText>(
				"row", text, font_name,
				centred(text, first_row_y +
					static_cast<float>(index) * row_spacing),
				resources));

			this->focus_.add(row, [this, choice]()
				{
					this->context()->pop(choice);
				});

			this->rows_[static_cast<std::size_t>(index)] = row;
		}

		// Adds are pending until a tick ends, and the frame drawn before the
		// first update() is a real frame - so whoever fills a scene ends its
		// first tick by hand (play_state.cpp says the same).
		this->scene_->end_tick();

		// The menu can open while a stick is already pushed - Start is on the
		// pad, and a thumb does not leave the stick to press it - and what is
		// pushed now is not a press. reset() would forget it and make the next
		// frame's push a fresh one, moving the cursor off the top row before
		// the screen is visible (docs/review/gpt6/README.md, G6-08). The push
		// has to be let go before this menu reads it.
		this->repeat_.start_held(this->held_direction());
	}

	void PauseState::arrange_screen()
	{
		this->arrangement_ =
			arrange(this->app_->resolution_manager()->resolution_vec());

		this->scene_->clear_views();
		this->scene_->add_view(this->arrangement_.viewport,
			this->arrangement_.camera);
	}

	UiText* PauseState::tapped_row() const
	{
		const Touch& touch = *this->app_->touch();

		for (const TouchPoint& point : touch.points())
		{
			if (!touch.pressed(point.id))
			{
				continue;
			}

			const Vector2F tap = this->arrangement_.camera.world_position(
				point.position, this->arrangement_.viewport);

			for (UiText* row : this->rows_)
			{
				const Vector2F centre = row->bounds().center();

				if (tap.x >= centre.x - row_tap_half_width &&
					tap.x < centre.x + row_tap_half_width &&
					tap.y >= centre.y - row_spacing * 0.5f &&
					tap.y < centre.y + row_spacing * 0.5f)
				{
					return row;
				}
			}
		}

		return nullptr;
	}

	Direction PauseState::held_direction() const
	{
		const Keyboard& keyboard = *this->app_->keyboard();

		// The keyboard first and held, not pressed: the repeat is
		// DirectionRepeat's job now, so every source below reports what is
		// being pushed and nothing computes an edge of its own. Two devices
		// disagreeing is not a case - a hand is on one of them.
		if (keyboard.held(Key::up))
		{
			return Direction::up;
		}

		if (keyboard.held(Key::down))
		{
			return Direction::down;
		}

		// Slot 0, spelt here rather than tracked, because this is a one-player
		// game - the same decision read_input() makes in play_state.cpp and
		// for the same reason.
		return pad_direction(this->app_->gamepads()->state(0));
	}

	bool PauseState::confirm_pressed() const
	{
		return this->app_->keyboard()->pressed(Key::enter) ||
			this->app_->keyboard()->pressed(Key::space) ||
			this->app_->gamepads()->pressed(0, GamepadButton::a);
	}

	bool PauseState::cancel_pressed() const
	{
		return this->app_->keyboard()->pressed(Key::escape) ||
			this->app_->keyboard()->pressed(Key::p) ||
			this->app_->gamepads()->pressed(0, GamepadButton::start) ||
			this->app_->gamepads()->pressed(0, GamepadButton::b);
	}

	void PauseState::update(float dt)
	{
		this->arrange_screen();

		// Cancel first, so that the key which opened the menu closes it
		// whatever the cursor is on. A player who paused by accident should not
		// have to read three rows to get back.
		if (this->cancel_pressed())
		{
			this->context()->pop(PauseChoice::resume);
			return;
		}

		const Direction direction =
			this->repeat_.update(this->held_direction(), dt);

		if (direction != Direction::none)
		{
			// The return value is "focus actually moved", which is the signal a
			// cursor sound hangs on (focus.h). This sample has no audio, so it
			// is deliberately ignored rather than plumbed to nothing.
			this->focus_.move(0, direction);
		}

		if (this->confirm_pressed())
		{
			this->focus_.activate(0);
		}
		else if (UiText* row = this->tapped_row(); row != nullptr)
		{
			// A tap is a cursor move and a confirm at once: the row goes
			// under the cursor first, so the menu shows what was chosen on
			// the frame it closes, and the action that runs is that row's.
			this->focus_.set_focused(0, row);
			this->focus_.activate(0);
		}

		// The scene still ticks: nothing here animates today, and a menu that
		// stopped updating its own widgets would be the shape a later one has
		// to undo.
		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void PauseState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer);
	}
}

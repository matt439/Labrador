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
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
#include "samples/linesweeper/web/test_input.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

using namespace labrador;

namespace linesweeper
{
	namespace
	{
		const std::string font_name = "jetbrains_mono_bold_16";
		const std::string block_texture_name = "white";

		// Explicit board-space row positions. FocusGroup initially focuses
		// the first added row, then uses measured bounds for navigation.
		constexpr float title_y = 236.0f;
		constexpr float first_row_y = 312.0f;
		constexpr float row_spacing = 44.0f;

		// Where the row at `index` from the top sits.
		constexpr float row_y(std::size_t index)
		{
			return first_row_y + static_cast<float>(index) * row_spacing;
		}

		// COLOURS is the third row, above QUIT.
		constexpr std::size_t colours_row_index = 2;

		constexpr Colour title_colour(200, 208, 222);

		// Touch targets use the full row gap and at least this half-width,
		// extending beyond the font's measured text bounds.
		constexpr float row_tap_half_width = 160.0f;

		// Wider text, including palette names, retains a margin on either side.
		constexpr float row_tap_margin = 16.0f;

		// What the COLOURS row says for `palette`.
		std::wstring colours_text(Palette palette)
		{
			return std::wstring(L"COLOURS: ") + palette_name(palette);
		}

		// Measure when text is set, then centre on the shared board-space
		// column. Floor the x-position to a whole unit: at the identity camera
		// a unit is a pixel and avoids half-pixel point-sampling ties for odd
		// text widths. Other buffer sizes scale through the camera; this
		// rounding alone cannot align every glyph texel at those scales.
		Vector2F centred(const RenderResources& resources, FontHandle font,
			const std::wstring& text, float y)
		{
			const Vector2F size = resources.measure_text(font, text);

			return Vector2F(std::floor((landscape_width - size.x) * 0.5f), y);
		}

		// How dark the match goes while the menu is up. Premultiplied black,
		// so it leaves a quarter of the board showing through and reads as a
		// pause rather than as a screen that replaced one (palette.h).
		constexpr float scrim_alpha = 0.75f;

		// A non-focusable dimming quad drawn from the sample's white texel.
		// Borrow the visible rectangle so resize updates both drawing and
		// culling. arrangement_ must outlive the scene holding this object.
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
					0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
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

	PauseState::PauseState(Application* app, Palette* palette) :
		app_(app),
		palette_(palette)
	{

	}

	void PauseState::init()
	{
		RenderResources* resources = this->app_->render_resources();
		this->font_ = resources->resolve_sprite_font(font_name);

		this->scene_ = std::make_unique<Scene>(nullptr, nullptr);

		// The match's arrangement, worked out the same way from the same
		// buffer, so the menu sits over the board whichever way up the screen
		// is (arrangement.h). Before the scrim, which borrows what it shows.
		this->arrange_screen();

		this->scene_->add(std::make_unique<Scrim>(
			resources->resolve_texture(block_texture_name),
			&this->arrangement_.visible));

		this->scene_->add(std::make_unique<UiText>("title", L"PAUSED",
			font_name, centred(*resources, this->font_, L"PAUSED", title_y),
			resources, title_colour));

		// The game reads input and asks FocusGroup to invoke the focused row's
		// action. A result pop is deferred until update() returns, preserving
		// this object and its callback while it runs. COLOURS instead changes
		// the borrowed palette and never submits its placeholder choice.
		struct Row
		{
			std::wstring text;
			PauseChoice choice;
		};

		const std::array<Row, 4> rows = { {
			{ L"RESUME", PauseChoice::resume },
			{ L"RESTART", PauseChoice::restart },
			{ colours_text(*this->palette_), PauseChoice::resume },
			{ L"QUIT", PauseChoice::quit },
		} };

		for (std::size_t index = 0; index < rows.size(); ++index)
		{
			const Row& entry = rows[index];

			UiText* row = this->scene_->add(std::make_unique<UiText>(
				"row", entry.text, font_name,
				centred(*resources, this->font_, entry.text, row_y(index)),
				resources));

			if (index == colours_row_index)
			{
				this->colours_row_ = row;
				this->focus_.add(row, [this]()
					{
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
						record_test_input_result("palette");
#endif
						this->select_palette(next_palette(*this->palette_));
					});
			}
			else
			{
				const PauseChoice choice = entry.choice;
				this->focus_.add(row, [this, choice]()
					{
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
						switch (choice)
						{
						case PauseChoice::resume:
							record_test_input_result("resume");
							break;
						case PauseChoice::restart:
							record_test_input_result("restart");
							break;
						case PauseChoice::quit:
							record_test_input_result("quit");
							break;
						}
#endif
						this->context()->pop(choice);
					});
			}

			this->rows_[index] = row;
		}

		// Commit pending objects before any draw, including one before update().
		this->scene_->end_tick();

		// A direction held when the menu opens must return to neutral before
		// it can move the initial cursor; reset() would treat it as a new push.
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

	void PauseState::select_palette(Palette palette)
	{
		*this->palette_ = palette;

		const std::wstring text = colours_text(palette);
		this->colours_row_->set_text(text);
		this->colours_row_->set_position(centred(
			*this->app_->render_resources(), this->font_, text,
			row_y(colours_row_index)));
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
				const RectangleF bounds = row->bounds();
				const Vector2F centre = bounds.center();
				const float half_width = std::max(row_tap_half_width,
					bounds.width * 0.5f + row_tap_margin);

				if (tap.x >= centre.x - half_width &&
					tap.x < centre.x + half_width &&
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

		// Arrow priority is up, down, left, right, then the pad. Read held
		// levels here; DirectionRepeat owns the navigation event timing.
		if (keyboard.held(Key::up))
		{
			return Direction::up;
		}

		if (keyboard.held(Key::down))
		{
			return Direction::down;
		}

		if (keyboard.held(Key::left))
		{
			return Direction::left;
		}

		if (keyboard.held(Key::right))
		{
			return Direction::right;
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
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
		TestInputStep test_step(*this->app_->keyboard(), "pause", this,
			[](const void* source)
			{
				const PauseState& state = *static_cast<const PauseState*>(source);
				int row = -1;
				for (std::size_t index = 0; index < state.rows_.size(); ++index)
				{
					if (state.focus_.focused(0) == state.rows_[index])
					{
						row = static_cast<int>(index);
						break;
					}
				}
				return TestInputSnapshot{ row, static_cast<int>(*state.palette_) };
			});
		if (!test_step.allowed()) return;
#endif
		this->arrange_screen();

		// Cancel first, so that the key which opened the menu closes it
		// whatever the cursor is on. A player who paused by accident should not
		// have to read four rows to get back.
		if (this->cancel_pressed())
		{
#if defined(LABRADOR_DEVELOPMENT_TEST_INPUT)
			test_step.result("resume");
#endif
			this->context()->pop(PauseChoice::resume);
			return;
		}

		const Direction direction =
			this->repeat_.update(this->held_direction(), dt);

		// Up and down walk the rows. Left and right change the palette when
		// the cursor is on COLOURS and do nothing anywhere else: the rows are
		// one column, so a sideways walk could only wrap the cursor somewhere
		// the player did not point.
		if (direction == Direction::up || direction == Direction::down)
		{
			// The return value is "focus actually moved", which is the signal a
			// cursor sound hangs on (focus.h). This sample has no audio, so it
			// is deliberately ignored rather than plumbed to nothing.
			this->focus_.move(0, direction);
		}
		else if ((direction == Direction::left ||
				direction == Direction::right) &&
			this->focus_.focused(0) == this->colours_row_)
		{
			this->select_palette(direction == Direction::left ?
				previous_palette(*this->palette_) :
				next_palette(*this->palette_));
		}

		if (this->confirm_pressed())
		{
			this->focus_.activate(0);
		}
		else if (UiText* row = this->tapped_row(); row != nullptr)
		{
			// Focus before activation so the action belongs to the touched row.
			this->focus_.set_focused(0, row);
			this->focus_.activate(0);
		}

		// Update this overlay's objects; the covered match is not updated.
		this->scene_->update(dt);
		this->scene_->end_tick();
	}

	void PauseState::draw(Renderer& renderer) const
	{
		this->scene_->draw(renderer);
	}
}

#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/input/direction.h"
#include "engine/scene/scene.h"
#include "engine/ui/focus.h"
#include "engine/ui/navigation.h"
#include "engine/ui/widget.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/palette.h"

#include <array>
#include <memory>

// A pause overlay built from measured UiText bounds, game-owned input and
// FocusGroup actions. covers_screen() leaves the match visible but suspended.
// StateContext defers pop() until the active callback returns, then delivers
// its typed result to PlayState after that state's on_resume().
//
// Layout is explicit: a title and four centred rows, with text measured when
// set. COLOURS changes PlayState's borrowed palette and stays open, so the
// board beneath the scrim shows the selection before the player resumes.
namespace linesweeper
{
	// What the pause screen decided, carried back to `PlayState` by
	// `StateContext::push<PauseChoice>`.
	enum class PauseChoice
	{
		resume,
		restart,
		quit,
	};

	class PauseState : public labrador::State
	{
	public:
		// `app` and `palette` are borrowed and must outlive this overlay. The
		// application's services and sample content must be ready before init().
		// COLOURS writes the palette during update(), before the const draw.
		PauseState(labrador::Application* app, Palette* palette);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

		// The match keeps drawing underneath, which is the one line that makes
		// this a pause menu rather than a screen.
		bool covers_screen() const override
		{
			return false;
		}

	private:
		// Held arrows take priority in up/down/left/right order. Without one,
		// pad_direction supplies slot zero's d-pad/stick direction; repeat_
		// turns the held direction into navigation events.
		labrador::Direction held_direction() const;

		// Whether the confirm or the cancel button went down this frame.
		bool confirm_pressed() const;
		bool cancel_pressed() const;

		// First row hit by a newly pressed finger, or null. Hit-test in board
		// space, using the row gap as height and at least the measured text's
		// width plus a margin. Input and hit-test policy stay outside engine/ui/.
		labrador::UiText* tapped_row() const;

		// The same arrangement PlayState draws the match through, worked out
		// again every tick (arrangement.h).
		void arrange_screen();

		// Writes `palette` into the borrowed selection and rewrites the
		// COLOURS row to name it, centred again on its new width.
		void select_palette(Palette palette);

		// Borrowed application/services and PlayState palette.
		labrador::Application* app_ = nullptr;
		Palette* palette_ = nullptr;

		// Resolved once in init(), for measuring the COLOURS row each time
		// it changes.
		labrador::FontHandle font_;

		// Which way up the screen is, refreshed every tick. Declared before
		// the scene because the scrim in it borrows the visible rectangle out
		// of this, and a member is destroyed after every member below it.
		Arrangement arrangement_;

		// The scrim, the title and the four rows. The scene owns them; the
		// focus group below holds loans into it, which is legal because the
		// scene outlives the group - both die with this object, members last.
		std::unique_ptr<labrador::Scene> scene_ = nullptr;

		// The four rows, for the tap test, and the one of them that names the
		// palette, for left and right and for rewriting. Loans into the
		// scene, like the focus group's.
		std::array<labrador::UiText*, 4> rows_ = {};
		labrador::UiText* colours_row_ = nullptr;

		// One focus slot for this one-player menu.
		labrador::FocusGroup focus_;

		// Turns "the stick is pushed up" into "move the cursor now", with the
		// long-then-short repeat a menu wants. It is a member because a repeat
		// is a clock and a clock is state; `start_held()` in `init()` is what
		// stops a stick that was already held when the menu opened from
		// stealing the first row.
		labrador::DirectionRepeat repeat_;
	};
}

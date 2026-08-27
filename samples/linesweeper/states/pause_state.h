#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/scene/scene.h"
#include "engine/ui/focus.h"
#include "engine/ui/navigation.h"
#include "engine/ui/widget.h"

#include <memory>

// A box over a running match, and the first client `engine/ui/` has ever had.
//
// THAT IS THE POINT OF THIS FILE, and it is worth being blunt about it.
// `grep -rn "engine/ui/" samples/` was empty: roughly 1,350 lines across eight
// files with no consumer anywhere in this repository, because the module's only
// client left with the split. `PHILOSOPHY.md` calls the samples "the permanent
// second client that keeps the boundary honest (T1)", and on this one module
// that client did not exist. The only exercise the widget set got was four
// `StubWidget`s in `tests/ui/`. `docs/next.md` section 3.3 is the finding.
//
// A STUB IS NOT A CLIENT, and the difference showed up immediately. A stub
// reports whatever bounds the test wants; a `UiText` reports the box the font
// measured, and a menu only navigates because those boxes are real. Nothing
// here needed the engine to change, which is the outcome the survey predicted
// and is worth recording as a pass rather than assuming.
//
// WHAT THE ENGINE ALREADY HAD. `State::covers_screen()` returning false is the
// whole of "draw the match underneath" - `state_context.h` names a pause menu
// as its worked example, twice, and this is the first one to exist.
// `StateContext::push<Result>` carries the answer back to `PlayState` on the
// stack frame rather than on a state object, and `pop()` is queued rather than
// immediate, which is what makes it safe to call from inside a button's action
// - the action, the `FocusGroup` holding it and this whole object are all
// destroyed by that pop, and would be destroyed mid-call if it were not
// deferred.
//
// The layout is manual (`PHILOSOPHY.md`, UI: "the game positions widgets
// explicitly"), and no autolayout crept in: three rows and a title, centred by
// measuring each string once in `init()`.
namespace linesweeper
{
	// What the pause screen decided, carried back to `PlayState` by
	// `StateContext::push<PauseChoice>`.
	//
	// An enum and not a bool, for the reason `focus.h` gives for `Activation`
	// being one: there are three answers and two of them are not "closed".
	enum class PauseChoice
	{
		resume,
		restart,
		quit,
	};

	class PauseState : public labrador::State
	{
	public:
		explicit PauseState(labrador::Application* app);

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
		// The frame's move, as a direction, from the keyboard and the d-pad.
		//
		// EDGES HERE, WHERE THE MATCH INSISTS ON held(). `play_state.cpp` reads
		// `held()` for every gameplay binding so that `tick()` derives its own
		// press edges and a recording carries them. A menu is not recorded and
		// not stepped, so an edge computed out here costs nothing and is what a
		// cursor wants: a held key that moved a cursor sixty times a second
		// would cross a three-row menu in a twentieth of one.
		//
		// It is nine lines of translation the engine ought to be doing, and
		// they are here on purpose. `navigation.h` says `Direction` is
		// "produced by the input module from a stick or a d-pad" and no such
		// producer exists - `grep -rn Direction engine/ | grep -v engine/ui/`
		// is empty. This function is what a client writes in its absence, and
		// it is the cheap two thirds: an edge-driven device needs no deadzone
		// and no repeat. The stick needs all three, which is why the stick is
		// not read here and why the producer lands with it.
		labrador::Direction read_direction() const;

		// Whether the confirm or the cancel button went down this frame.
		bool confirm_pressed() const;
		bool cancel_pressed() const;

		// Borrowed. The shell built every service before this existed and
		// outlives it (PHILOSOPHY, Services and lifetimes).
		labrador::Application* app_ = nullptr;

		// The scrim, the title and the three rows. The scene owns them; the
		// focus group below holds loans into it, which is legal because the
		// scene outlives the group - both die with this object, members last.
		std::unique_ptr<labrador::Scene> scene_ = nullptr;

		// One slot. Split-screen is why `FocusGroup` has them at all
		// (focus.h), and a one-player falling-block game is the case that
		// passes 0 and never thinks about it again.
		labrador::FocusGroup focus_;
	};
}

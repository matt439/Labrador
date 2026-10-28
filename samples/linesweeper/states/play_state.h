#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/presentation/touch_controls.h"
#include "samples/linesweeper/states/pause_state.h"
#include "samples/linesweeper/states/resume_gate.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <memory>

// Game-owned input, match updates and pause policy. rules/ has no engine
// dependency; presentation/ reads a World; states/ turns device input into
// rules verbs. cmake/check_linesweeper_layers.cmake enforces literal includes:
// no engine/ from rules/, and no rules/tick.h from presentation/. It does not
// analyse transitive dependencies or macro includes (README, Three layers).
namespace linesweeper
{
	class PlayState : public labrador::State
	{
	public:
		// `app` is borrowed and must outlive this state; initialize it and load
		// the sample's content before init(). `motion` selects the particle
		// count; the browser entry supplies the system preference.
		//
		// `report_palette`, if given, is called with the palette the match
		// starts in and when the pause menu closes on a different one,
		// including when QUIT is chosen. The browser entry logs the report.
		explicit PlayState(labrador::Application* app,
			Motion motion = Motion::full,
			void (*report_palette)(Palette) = nullptr);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

		// The pause menu has closed over this state. Whatever is held right
		// now was held to close it - or was held since before it opened - and
		// none of it is the match's until it is let go (resume_gate.h).
		void on_resume() override;

		// Focus loss opens the same pause menu as an explicit pause. A covered
		// match is already paused, so activation changes must not stack menus.
		void on_suspend() override;
		void on_deactivated() override;

	private:
		// OR the held keyboard, slot-zero pad and touch controls into tick.h's
		// button mask. Bindings and pause policy belong to the game (T1).
		std::uint8_t read_input() const;

		// Push a typed pause screen. StateContext delivers its result after
		// on_resume(), when the resumed state and its input gate are ready.
		void open_pause_menu();

		// Refresh the scene's view and touch hit-test geometry from the current
		// buffer before each play update and when the pause menu closes.
		void arrange_screen();

		// Shows the two keyboard and pad hint lines in landscape until a
		// finger has been seen, and hides them otherwise.
		void show_hints();

		// Borrowed application and services; see the constructor's lifetime rule.
		labrador::Application* app_ = nullptr;
		Motion motion_ = Motion::full;

		// Board and particles borrow the palette for const draws. The pause
		// menu writes it during update(), before drawing, so a selection is
		// visible under its scrim. RESTART preserves the player's selection.
		Palette palette_ = standard_palette;

		// The palette report_palette was last called with, so it is called
		// on a change and not on every close of the menu.
		void (*report_palette_)(Palette) = nullptr;
		Palette reported_palette_ = standard_palette;

		// The match is a value. Both restart paths assign World{} in place,
		// preserving the addresses borrowed by presentation objects.
		World world_;

		// What the latest tick() said, beside the World it said it about. The
		// particle field borrows both (particles.h): the World is the match
		// and this is the one thing about the last step of it that the match
		// does not keep.
		TickResult last_tick_;

		// Between read_input() and tick(): the keys the pause menu was closed
		// with, kept out of the byte until released. Not part of World,
		// because it is not part of the match - a recording of the bytes
		// this lets through replays without it.
		ResumeGate gate_;

		// Scene owns its objects; the pointers here are borrowed. It is declared
		// after their World, TickResult and palette loans, so it dies first.
		std::unique_ptr<labrador::Scene> scene_ = nullptr;
		BoardView* board_ = nullptr;

		// One object holds the particle field. Scene registration puts its
		// sprites over the board without sorting by layer_depth.
		ParticleField* particles_ = nullptr;

		// Registered after the particles so the top-out message draws over them.
		TopOutBanner* banner_ = nullptr;
		labrador::Label* hint_ = nullptr;
		labrador::Label* pad_hint_ = nullptr;

		// Last of all, over everything, and drawn only once touch_seen_ is
		// set (touch_controls.h).
		TouchControls* controls_ = nullptr;

		// Which way up the screen is this tick, and the camera that follows.
		// The touch controls are hit-tested against the same value they are
		// drawn from, so a finger and the button under it cannot disagree.
		Arrangement arrangement_;

		// Whether a finger has ever touched the screen. From then on the
		// controls are drawn, the keyboard hints are not, and the top-out
		// banner asks for a tap. It does not go back: a player who touched
		// once and reached for a keyboard loses nothing by the buttons
		// staying, and one who put the phone down for a second should not
		// find them gone.
		bool touch_seen_ = false;

		// Whether the two hint lines are showing, so they are rewritten only
		// on the tick that changes it.
		bool hints_shown_ = true;
		bool covered_ = false;
	};
}

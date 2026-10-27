#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/arrangement.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/presentation/touch_controls.h"
#include "samples/linesweeper/states/pause_state.h"
#include "samples/linesweeper/states/resume_gate.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <memory>

// The falling-block sample's one live state, and the whole of the game loop.
//
// THIS IS ONE OF THE ONLY TWO PLACES ALLOWED TO INCLUDE BOTH engine/ AND
// rules/, and the only one that includes rules/tick.h. The layering is that
// rules/ knows nothing of the engine, presentation/ reads a World and draws
// it, and states/ is where the two meet - which in practice means the three
// tables and three loops below that turn a keyboard, a pad and the touch
// controls into one byte. It
// is enforced by review today and by a build check once presentation/ has
// enough files to make one worth writing (README, Still open).
namespace linesweeper
{
	class PlayState : public labrador::State
	{
	public:
		// `motion` is how much the particle field throws (particles.h): the
		// browser build passes what the page's reduced-motion query said.
		explicit PlayState(labrador::Application* app,
			Motion motion = Motion::full);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

		// The pause menu has closed over this state. Whatever is held right
		// now was held to close it - or was held since before it opened - and
		// none of it is the match's until it is let go (resume_gate.h).
		void on_resume() override;

	private:
		// Every device as one of tick.h's button masks. A method on the state
		// rather than a class, because it is three tables and three loops over
		// them, and the engine deliberately has no action-mapping layer to put
		// it in (AGENTS.md, Known-absent). What that costs is measured rather
		// than asserted - README, Still open, has the number.
		std::uint8_t read_input() const;

		// Pushes the pause screen and acts on what it decided.
		//
		// The answer comes back through StateContext::push<PauseChoice>, which
		// carries it on the stack frame rather than on either state - so
		// neither this class nor PauseState holds a result member, and there
		// is nothing to reset when the menu is opened a second time
		// (state_context.h).
		void open_pause_menu();

		// Works out the arrangement for the buffer as it is now and points
		// the scene's one view through it (arrangement.h). Every tick, so a
		// resize or a phone turned on its side is followed on the next frame;
		// it is a handful of divisions.
		void arrange_screen();

		// Shows the two keyboard and pad hint lines in landscape until a
		// finger has been seen, and hides them otherwise.
		void show_hints();

		// Borrowed. The shell built every service before this existed and
		// outlives it (PHILOSOPHY, Services and lifetimes).
		labrador::Application* app_ = nullptr;
		Motion motion_ = Motion::full;

		// The match, by value, as a member. Not a unique_ptr, not a handle,
		// not registered with anything - 276 bytes that die when this state
		// does, and restarting is `this->world_ = World{};` on one line of
		// update() below.
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

		// The scene owns what it is given; these are the pointers add() hands
		// back for the things the state still has something to say to. The
		// board holds a `const World*` into the member above, which is legal
		// because the state outlives its own scene.
		std::unique_ptr<labrador::Scene> scene_ = nullptr;
		BoardView* board_ = nullptr;

		// One object, ten thousand particles. It reads the same World the
		// board does and is registered after it, which is the whole of the
		// ordering: the scene draws in the order it was given, so sparks land
		// over the stack without a depth or a sort (particles.h).
		ParticleField* particles_ = nullptr;

		// Registered after the field, and that is the whole reason it is not
		// part of the board: object order is this sample's only depth, and the
		// words a player has to read are thrown over by the loudest burst the
		// field owns (top_out_banner.h).
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
	};
}

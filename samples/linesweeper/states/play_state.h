#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/states/pause_state.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <memory>

// The falling-block sample's one live state, and the whole of the game loop.
//
// THIS IS ONE OF THE ONLY TWO PLACES ALLOWED TO INCLUDE BOTH engine/ AND
// rules/, and the only one that includes rules/tick.h. The layering is that
// rules/ knows nothing of the engine, presentation/ reads a World and draws
// it, and states/ is where the two meet - which in practice means the two
// tables and two loops below that turn a keyboard and a pad into one byte. It
// is enforced by review today and by a build check once presentation/ has
// enough files to make one worth writing (README, Still open).
namespace linesweeper
{
	class PlayState : public labrador::State
	{
	public:
		explicit PlayState(labrador::Application* app);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

	private:
		// Both devices as one of tick.h's button masks. A method on the state
		// rather than a class, because it is two tables and two loops over
		// them, and the engine deliberately has no action-mapping layer to put
		// it in (CLAUDE.md, Known-absent). What that costs is measured rather
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

		// Borrowed. The shell built every service before this existed and
		// outlives it (PHILOSOPHY, Services and lifetimes).
		labrador::Application* app_ = nullptr;

		// The match, by value, as a member. Not a unique_ptr, not a handle,
		// not registered with anything - 276 bytes that die when this state
		// does, and restarting is `this->world_ = World{};` on one line of
		// update() below.
		World world_;

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
	};
}

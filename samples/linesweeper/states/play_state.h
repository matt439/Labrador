#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <memory>

// The falling-block sample's one live state, and the whole of the game loop.
//
// THIS IS ONE OF THE ONLY TWO PLACES ALLOWED TO INCLUDE BOTH engine/ AND
// rules/, and the only one that includes rules/tick.h. The layering is that
// rules/ knows nothing of the engine, presentation/ reads a World and draws
// it, and states/ is where the two meet - which in practice means the seven
// lines of update() below that turn a keyboard into a byte. It is enforced by
// review today and by a build check once presentation/ has enough files to
// make one worth writing (README, Still open).
namespace linesweeper
{
	class PlayState : public artattack::State
	{
	public:
		explicit PlayState(artattack::Application* app);

		void init() override;
		void update(float dt) override;
		void draw(artattack::Renderer& renderer) const override;

	private:
		// The keyboard as one of tick.h's button masks. A free function on the
		// state rather than a class, because it is a switch over eight keys
		// and the engine deliberately has no action-mapping layer to put it in
		// (CLAUDE.md, Known-absent).
		std::uint8_t read_input() const;

		// Borrowed. The shell built every service before this existed and
		// outlives it (PHILOSOPHY, Services and lifetimes).
		artattack::Application* app_ = nullptr;

		// The match, by value, as a member. Not a unique_ptr, not a handle,
		// not registered with anything - 276 bytes that die when this state
		// does, and restarting is `this->world_ = World{};` on one line of
		// update() below.
		World world_;

		// The scene owns what it is given; these are the pointers add() hands
		// back for the things the state still has something to say to. The
		// board holds a `const World*` into the member above, which is legal
		// because the state outlives its own scene.
		std::unique_ptr<artattack::Scene> scene_ = nullptr;
		BoardView* board_ = nullptr;
		artattack::Label* hint_ = nullptr;
	};
}

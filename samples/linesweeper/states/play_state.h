#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/rules/world.h"

#include <memory>

// The falling-block sample's one live state.
//
// A STUB. It owns the World and draws nothing of it yet: the rules layer is
// types and contracts so far, and tick() lands in the commit after this one.
// What this file is currently good for is proving the wiring - that the target
// builds, that the rules library links into it, that the manifest is found and
// that a frame reaches the screen.
//
// THIS IS ONE OF THE ONLY TWO PLACES ALLOWED TO INCLUDE BOTH engine/ AND
// rules/. The layering is that rules/ knows nothing of the engine,
// presentation/ reads a World and draws it, and states/ is where the two meet
// and where input becomes a rules verb. It is enforced by review today and by
// a build check once there is enough here to check.
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
		// Borrowed. The shell built every service before this existed and
		// outlives it (PHILOSOPHY, Services and lifetimes).
		artattack::Application* app_ = nullptr;

		// The match, by value, as a member. Not a unique_ptr, not a handle,
		// not registered with anything - 276 bytes that die when this state
		// does. Restarting it will be `this->world_ = World{};` and nothing
		// else.
		World world_;

		// The scene owns what it is given; these are the pointers add() hands
		// back for the things the state still has something to say to.
		std::unique_ptr<artattack::Scene> scene_ = nullptr;
		artattack::Label* title_ = nullptr;
		artattack::Label* status_ = nullptr;
	};
}

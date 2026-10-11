#pragma once

#include "samples/collision/arena.h"

#include "engine/app/application.h"
#include "engine/core/state.h"

#include <memory>
#include <string>

namespace collisiondemo
{
	// The sample's one screen: the arena, moved by the keys or a stick, and the
	// labels that say what each rectangle is. main.cpp and main_web.cpp both
	// run it; only main.cpp drives it as a smoke test.
	class CollisionState final : public labrador::State
	{
	public:
		// A smoke test replaces the player's input with a fixed route into the
		// wall and around it to the trigger, which verify_smoke_test() checks.
		CollisionState(labrador::Application* app, bool smoke_test);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

		// Throws unless the smoke route met the wall, entered the trigger once
		// and ended where it should.
		void verify_smoke_test() const;

	private:
		void refresh_view();

		// The application owns this state during normal play and outlives it.
		labrador::Application* app_;
		bool smoke_test_;
		std::unique_ptr<Arena> arena_;
		labrador::FontHandle font_;
		std::wstring status_ = L"Contacts this tick: 0    Trigger entries: 0";
		int ticks_ = 0;
	};
}

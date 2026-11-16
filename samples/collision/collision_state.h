#pragma once

#include "samples/collision/arena.h"
#include "samples/components/shape_outline.h"

#include "engine/app/application.h"
#include "engine/core/state.h"

#include <memory>
#include <string>

namespace collision
{
	// The smoke test's fixed step, and how many of them main.cpp runs: the
	// route ends at smoke_route_ticks; the remaining ticks keep the player
	// still while the trigger-entry marker expires.
	inline constexpr float smoke_step_time = 1.0f / 60.0f;
	inline constexpr int smoke_route_ticks = 315;
	inline constexpr int smoke_test_ticks = smoke_route_ticks + 60;

	// The sample's one screen: the arena, moved by the keys or a stick, and the
	// labels that say what each rectangle is. main.cpp and main_web.cpp both
	// run it; only main.cpp drives it as a smoke test.
	class CollisionState final : public labrador::State
	{
	public:
		// A smoke test replaces the player's input with a fixed route into the
		// wall and around it to the trigger, which verify_smoke_test() checks.
		CollisionState(labrador::Application* app, bool smoke_test,
			bool show_outlines = false);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

		// Throws unless the route met the wall, entered the trigger once,
		// spawned and retired its marker, and ended where it should.
		void verify_smoke_test() const;

	private:
		void refresh_view();
		void draw_outlines(labrador::DrawList& draw_list) const;

		// The application owns this state during normal play and outlives it.
		labrador::Application* app_;
		bool smoke_test_;
		bool show_outlines_;
		components::ShapeOutline outlines_;
		std::unique_ptr<Arena> arena_;
		labrador::FontHandle font_;
		std::wstring status_ = L"Contacts this tick: 0    Trigger entries: 0    Entry markers: 0";
		int ticks_ = 0;
	};
}

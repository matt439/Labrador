#pragma once

#include "engine/core/state.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/rules/world.h"
#include "samples/linesweeper/web/pacing_observer.h"

namespace labrador
{
	class Application;
}

namespace linesweeper
{
	class ParticleField;

	// An opt-in development fixture, not a game mode or an input/replay API.
	// It fills the benchmark's well, waits 300 fixed steps, then creates the
	// exact first burst. Its draw remains a pure read through the normal scene.
	// The profile keeps the bench's fixed landscape hint labels and omits touch
	// controls, even when the responsive camera frames a portrait buffer.
	class PacingState final : public labrador::State
	{
	public:
		PacingState(labrador::Application* app, PacingSnapshot* snapshot, bool frozen);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

	private:
		void arrange_screen();

		labrador::Application* app_ = nullptr;
		PacingSnapshot* snapshot_ = nullptr;
		World world_;
		TickResult last_tick_;
		labrador::Scene scene_{ nullptr, nullptr };
		ParticleField* particles_ = nullptr;
		int warmup_updates_ = 0;
		bool frozen_ = false;
	};
}

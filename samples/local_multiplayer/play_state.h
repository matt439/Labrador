#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/scene/scene.h"
#include "samples/local_multiplayer/arena.h"
#include "samples/local_multiplayer/join_rules.h"

#include <array>
#include <memory>
#include <string>

namespace multiplayer
{
	// The font both screens draw with, as content/manifest.json names it.
	inline const std::string font_name = "jetbrains_mono_bold_16";

	// The smoke test's fixed step, and how many of them main.cpp runs before
	// verify_smoke_test().
	inline constexpr float smoke_step_time = 1.0f / 60.0f;
	inline constexpr int smoke_ticks = 120;

	// Selects PlayState's smoke-test constructor.
	struct SmokeTest
	{
	};

	class PlayState final : public labrador::State
	{
	public:
		// A match: each player reads only the source it joined with, and
		// Escape, or B on a joined pad, quits.
		PlayState(labrador::Application* app,
			const std::array<InputSource, Arena::player_count>& sources);

		// main.cpp's smoke test: no menu and no input. Both players hold a
		// fixed direction every tick, nothing quits, and verify_smoke_test()
		// checks where they end.
		PlayState(labrador::Application* app, SmokeTest);
		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;
		void verify_smoke_test() const;

	private:
		void rebuild_views();
		// The application owns this state and outlives it.
		labrador::Application* app_;
		bool smoke_test_ = false;
		// Match assignments remain fixed. An absent pad contributes neutral
		// input; replugging is observed as slot occupancy, not device identity.
		std::array<InputSource, Arena::player_count> sources_ = {
			InputSource::keyboard_left, InputSource::keyboard_right
		};
		std::unique_ptr<labrador::Scene> scene_;
		// Owned by scene_, which retains the arena for this state's lifetime.
		Arena* arena_ = nullptr;
		labrador::TextureHandle white_;
		labrador::FontHandle font_;
	};
}

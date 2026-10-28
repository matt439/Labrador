#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/scene/scene.h"
#include "samples/local_multiplayer/arena.h"
#include "samples/local_multiplayer/join_rules.h"

#include <array>
#include <memory>

namespace multiplayer
{
	class PlayState final : public labrador::State
	{
	public:
		explicit PlayState(labrador::Application* app, bool smoke_test = false);
		PlayState(labrador::Application* app,
			const std::array<InputSource, Arena::player_count>& sources);
		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;
		void verify_smoke_test() const;

	private:
		void rebuild_views();
		// The application owns this state and outlives it.
		labrador::Application* app_;
		bool smoke_test_;
		bool bound_sources_ = false;
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

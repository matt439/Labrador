#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/scene/scene.h"
#include "samples/local_multiplayer/arena.h"

#include <memory>

namespace multiplayer
{
	class PlayState final : public labrador::State
	{
	public:
		explicit PlayState(labrador::Application* app, bool smoke_test = false);
		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;
		void verify_smoke_test() const;

	private:
		void rebuild_views();
		// The application owns this state and outlives it.
		labrador::Application* app_;
		bool smoke_test_;
		std::unique_ptr<labrador::Scene> scene_;
		// Owned by scene_, which retains the arena for this state's lifetime.
		Arena* arena_ = nullptr;
		labrador::TextureHandle white_;
		labrador::FontHandle font_;
	};
}

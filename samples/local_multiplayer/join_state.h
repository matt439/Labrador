#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/input/direction.h"
#include "engine/scene/scene.h"
#include "engine/ui/focus.h"
#include "engine/ui/widget.h"
#include "samples/local_multiplayer/join_rules.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace multiplayer
{
	// Two players bind sources and choose READY independently. Source ownership
	// and disconnect rules live in Lobby; engine UI supplies focus and repeat.
	class JoinState final : public labrador::State
	{
	public:
		// Borrow app; its initialized services and sample content must be ready
		// before init(), and it must outlive this state.
		explicit JoinState(labrador::Application* app);
		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;
		const Lobby& lobby() const;

	private:
		void rebuild_views();
		labrador::Direction held_direction(InputSource source) const;
		bool confirm_pressed(InputSource source) const;
		bool leave_pressed(InputSource source) const;

		// Borrowed application and services.
		labrador::Application* app_;
		Lobby lobby_;
		std::unique_ptr<labrador::Scene> scene_;
		std::array<labrador::UiText*, 2> rows_ = {};
		// Widget loans must be destroyed before scene_. Each player has its own
		// focus index and repeat clock, even when both cursors share one row.
		labrador::FocusGroup focus_;
		std::array<labrador::DirectionRepeat, Arena::player_count> repeats_;
		std::size_t activating_player_ = 0;
		labrador::TextureHandle white_;
		labrador::FontHandle font_;
		std::wstring connection_notice_;
	};
}

#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/audio/sound_board.h"

#include <memory>

namespace audio
{
	class AudioState final : public labrador::State
	{
	public:
		explicit AudioState(labrador::Application* app);
		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;
		void on_deactivated() override;
		void on_activated() override;

	private:
		// Borrowed from the application that owns this state and its resources.
		labrador::Application* app_;
		std::unique_ptr<SoundBoard> board_;
		labrador::Scene scene_{ nullptr, nullptr };
		// Owned by scene_ and retained until this state is destroyed.
		labrador::Label* status_ = nullptr;
		bool resume_loop_ = false;

		// What the status line shows, so that refresh_status() rebuilds and
		// re-measures it only when one of them changes rather than every tick
		// (engine/render/label.h).
		struct StatusValues
		{
			labrador::SoundState loop = labrador::SoundState::stopped;
			float volume = 0.0f;
			float pan = 0.0f;
			float pitch = 0.0f;
		};
		StatusValues shown_;
		bool status_shown_ = false;
		void refresh_status();
	};
}

#pragma once

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/label.h"
#include "engine/scene/scene.h"
#include "samples/audio/sound_board.h"

#include <memory>

namespace audio_sample
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
		void refresh_status();
	};
}

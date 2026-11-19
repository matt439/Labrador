#pragma once

#include "samples/components/touch_button.h"
#include "samples/components/touch_stick.h"

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/render/camera.h"
#include "engine/render/viewport.h"

#include <functional>
#include <string>

namespace components
{
	struct TouchDemoReading
	{
		bool a_held = false;
		bool b_held = false;
		labrador::Vector2F stick = labrador::Vector2F::zero;
		bool operator==(const TouchDemoReading&) const = default;
	};

	// An internal gallery page, not another sample or a reusable component.
	// It chooses regions, fresh-contact precedence and bindings for the two
	// copied input values. The fitted camera converts client pixels once.
	class TouchDemoState final : public labrador::State
	{
	public:
		using StatusChanged = std::function<void(const std::wstring&)>;
		TouchDemoState(labrador::Application* app, StatusChanged on_status = nullptr);
		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;
		void on_deactivated() override;
		void on_suspend() override;
		void on_resume() override;
		TouchDemoReading reading() const;

	private:
		bool refresh_view();
		labrador::Vector2F world_point(const labrador::Vector2I& client) const;
		bool begin_contact(std::uint32_t contact, const labrador::Vector2F& position);
		bool update_touch();
		bool update_mouse();
		void update_cursor();
		void cancel_controls();
		void publish_status();
		void leave();

		// The owning application outlives this page and all borrowed resources.
		labrador::Application* app_;
		StatusChanged on_status_;
		TouchStick stick_;
		TouchButton a_;
		TouchButton b_;
		TouchButton back_;
		labrador::Viewport viewport_;
		labrador::Camera camera_;
		bool view_ready_ = false;
		bool wait_for_clear_ = false;
		bool mouse_active_ = false;
		std::uint64_t mouse_epoch_ = 0;
		std::uint64_t input_epoch_ = 0;
		int mouse_quiet_ticks_ = 0;
		labrador::TextureHandle white_;
		labrador::FontHandle font_;
		bool reported_ = false;
		TouchDemoReading reported_reading_;
		std::wstring message_;
	};
}

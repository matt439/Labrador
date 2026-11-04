#pragma once

#include "samples/components/animation_preview.h"
#include "samples/components/option_row.h"
#include "samples/components/pointer_capture.h"
#include "samples/components/slider.h"

#include "engine/app/application.h"
#include "engine/core/state.h"
#include "engine/input/direction.h"
#include "engine/scene/scene.h"
#include "engine/ui/focus.h"
#include "engine/ui/widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace components
{
	// Every component on one settings screen, so each can be seen and played
	// with: two option rows, two sliders, two animations and a way out.
	// main.cpp and main_web.cpp both run it.
	//
	// It is also the page half a component leaves to its game. A component
	// reads no input, so this is where up and down move the cursor, where left
	// and right reach the focused row, where a click or a tap finds a row and
	// a drag moves a slider, and where changes reach the game.
	// The window row requests Application's mode and follows its reports;
	// difficulty and volume changes illustrate callbacks through a status line.
	class GalleryState final : public labrador::State
	{
	public:
		// Told every line the status shows, for a page that reports it;
		// main_web.cpp writes each to the console.
		using StatusChanged = std::function<void(const std::wstring& text)>;

		explicit GalleryState(labrador::Application* app,
			StatusChanged on_status = nullptr);

		void init() override;
		void update(float dt) override;
		void draw(labrador::Renderer& renderer) const override;

	private:
		// Which way the player is pushing, from the arrows and either pad
		// direction, for the repeat below to turn into presses.
		labrador::Direction held_direction() const;

		// Sends left or right to whichever component the cursor is on.
		//
		// BY COMPARING POINTERS, because the page built every row and holds
		// each one already: asking the focus group which widget it is on and
		// matching it here needs no cast and no table. A page with many rows
		// of one kind would loop over a list of them instead.
		bool adjust_focused(labrador::Direction direction);

		// What one tick's pointer did to the rows: a press focuses the row
		// under it and starts dragging a slider; a release completes a click
		// or a tap only on the row it began on (pointer_capture.h).
		void update_pointer();

		// The world point under a client pixel, through the camera the page
		// draws with.
		labrador::Vector2F world_point(const labrador::Vector2I& client) const;

		void set_status(const std::wstring& text);

		void refresh_view();

		// Borrowed. The shell built every service before this existed and
		// outlives it.
		labrador::Application* app_ = nullptr;

		// Owns the panel, labels and rows. The widget pointers and focus targets
		// below are loans into it; the scene, declared first, dies last.
		std::unique_ptr<labrador::Scene> scene_ = nullptr;

		// Values borrowing Application's resource table. The scene's const
		// overlay draws them through each view without advancing their clocks.
		std::optional<AnimationPreview> loop_preview_;
		std::optional<AnimationPreview> once_preview_;

		OptionRow* difficulty_ = nullptr;
		OptionRow* window_mode_ = nullptr;
		Slider* music_ = nullptr;
		Slider* effects_ = nullptr;
		labrador::UiText* status_ = nullptr;

		labrador::FocusGroup focus_;
		labrador::DirectionRepeat repeat_;

		// The rows a pointer can choose, in focus order, and the widget each
		// one is; pointer_widgets_[i] answers for pointer_targets_[i].
		std::vector<PointerTarget> pointer_targets_;
		std::vector<labrador::UiWidget*> pointer_widgets_;

		// The one contact followed, and the row it began on.
		PointerCapture pointer_;
		labrador::UiWidget* pressed_row_ = nullptr;
		Slider* dragging_ = nullptr;

		StatusChanged on_status_;
	};
}

#pragma once

#include "engine/render/screen_layout.h"
#include "engine/render/resolution_manager.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/viewport.h"

#include <vector>

namespace labrador
{
	class ViewportManager
	{
	public:
		// Takes no device, and no member of this class touches one: a
		// ResolutionManager needs none, and every member here hands out a
		// rectangle. That is what makes it constructible in a test.
		explicit ViewportManager(ResolutionManager* resolution_manager);

		void set_layout(ScreenLayout layout);
		ScreenLayout layout() const { return layout_; }

		// Pure arithmetic, all of it. Applying a viewport to a device is
		// DrawList::set_viewport, which is why this class names no graphics
		// type. What a viewport shows of the world through a camera is
		// Camera::visible_rectangle, which is why it names no Camera either.
		Viewport player_viewport(int player_num) const;

		std::vector<Viewport> all_viewports() const;

		// The strips between the panes, in the same pixels as the viewports,
		// and none for a one-player layout. What fills them is the game's
		// business: this class hands out rectangles and draws nothing.
		std::vector<mattmath::RectangleF> viewport_dividers() const;

		Viewport fullscreen_viewport() const;

	private:
		static constexpr float DIVIDER_THICKNESS = 2.0f;

		ResolutionManager* resolution_manager_ = nullptr;

		ScreenLayout layout_ = ScreenLayout::one_player;

		int player_count_from_layout(ScreenLayout layout) const;
		int viewport_count_from_layout(ScreenLayout layout) const;

		Viewport calculate_viewport(ScreenLayout layout,
			int player_num, const mattmath::Vector2F& screen_size) const;
	};
}

#pragma once

#include "engine/render/screen_layout.h"
#include "engine/render/resolution_manager.h"
#include "engine/math/matt_math.h"
#include "engine/math/colour.h"

namespace artattack
{
	class ViewportManager
	{
	public:
		// How a split-screen divider is drawn. They live on the class that shapes
		// the dividers rather than in a `consts` namespace of strays (CONVENTIONS,
		// Constants and enumerators); a game asks for the rectangles and then for
		// these to fill them.
		static inline const std::string DIVIDER_SHEET_NAME = "sprite_sheet_1";
		static inline const std::string DIVIDER_FRAME_NAME = "pixel";
		static inline const mattmath::Colour DIVIDER_COLOUR = colour_consts::BLACK;

		// Takes no DeviceResources. It held one and no member function ever
		// read it, and it was the only thing standing between this class and
		// a test: a ResolutionManager needs no device, so a ViewportManager
		// now needs no device either. The two members that do touch D3D take
		// the context as a parameter.
		explicit ViewportManager(ResolutionManager* resolution_manager);

		void set_layout(ScreenLayout layout);
		ScreenLayout layout() const { return layout_; }

		// Pure arithmetic, all of it. The two apply_player_viewport overloads
		// that used to live here were three lines of RSSetViewports and
		// SpriteBatch::SetViewport apiece, and they are DrawList::set_viewport
		// now - which is why this class no longer names a graphics type and can
		// be constructed in a test.
		mattmath::Viewport player_viewport(int player_num) const;

		std::vector<mattmath::Viewport> all_viewports() const;

		mattmath::RectangleF camera_adjusted_player_viewport_rect(
			int player_num, const mattmath::Camera& camera) const;

		std::vector<mattmath::RectangleF> viewport_dividers() const;

		mattmath::Viewport fullscreen_viewport() const;

	private:
		static constexpr float DIVIDER_THICKNESS = 2.0f;

		ResolutionManager* resolution_manager_ = nullptr;

		ScreenLayout layout_ = ScreenLayout::one_player;

		int player_count_from_layout(ScreenLayout layout) const;
		int viewport_count_from_layout(ScreenLayout layout) const;

		mattmath::Viewport calculate_viewport(ScreenLayout layout,
			int player_num, const mattmath::Vector2F& screen_size) const;
	};
}

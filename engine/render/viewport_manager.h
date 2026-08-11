#pragma once

#include "engine/render/screen_layout.h"
#include "engine/render/resolution_manager.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/viewport.h"

#include <vector>

namespace artattack
{
	class ViewportManager
	{
	public:
		// How a split-screen divider is *drawn* is not here any more. It was
		// three constants naming "sprite_sheet_1", the frame "pixel" and a
		// colour - the paint-shooter's own asset names, inside a module that
		// is not allowed to know this game exists, and DIVIDER_COLOUR was an
		// inline variable initialised from a constant in another translation
		// unit, so its value depended on initialisation order. viewport_dividers()
		// hands out rectangles; what fills them is the game's business, and its
		// one caller now says so itself.

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
		Viewport player_viewport(int player_num) const;

		std::vector<Viewport> all_viewports() const;

		// camera_adjusted_player_viewport_rect(player_num, camera) is gone. It
		// asked the layout for a viewport its only caller had just been handed
		// one of, and the rest of it was the camera's inverse transform written
		// a second time - which is how the wrong copy of that inverse, a
		// multiply where the arithmetic divides, survived two reviews and a
		// seam extraction. Camera::visible_rectangle states it once, beside the
		// forward transform, and this class needs no Camera at all now.
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

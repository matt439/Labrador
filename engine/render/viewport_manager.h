#pragma once

#include "engine/render/screen_layout.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/device_resources.h"
#include "engine/math/matt_math.h"
#include "engine/math/colour.h"
#include "SpriteBatch.h"

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

		// Every overload takes the context to apply to. There used to be a
		// one-argument version that reached for the IMMEDIATE context and a cached
		// SpriteBatch*, which render workers were calling - ID3D11DeviceContext is
		// not thread-safe, and the deferred contexts exist precisely so that
		// workers never touch the immediate one.
		void apply_player_viewport(int player_num,
			ID3D11DeviceContext* context,
			DirectX::SpriteBatch* sprite_batch) const;
		void apply_player_viewport(int player_num,
			ID3D11DeviceContext* context) const;

		mattmath::Viewport player_viewport(int player_num) const;

		std::vector<mattmath::Viewport> all_viewports() const;

		mattmath::RectangleF camera_adjusted_player_viewport_rect(
			int player_num, const mattmath::Camera& camera) const;

		std::vector<mattmath::RectangleF> viewport_dividers() const;

		D3D11_VIEWPORT fullscreen_d3d11_viewport() const;

	private:
		static constexpr float DIVIDER_THICKNESS = 2.0f;

		ResolutionManager* resolution_manager_ = nullptr;

		ScreenLayout layout_ = ScreenLayout::one_player;

		int player_count_from_layout(ScreenLayout layout) const;
		int viewport_count_from_layout(ScreenLayout layout) const;

		D3D11_VIEWPORT calculate_d3d11_viewport(ScreenLayout layout,
			int player_num, const mattmath::Vector2F& screen_size) const;
		mattmath::Viewport calculate_viewport(ScreenLayout layout,
			int player_num, const mattmath::Vector2F& screen_size) const;

		mattmath::Viewport fullscreen_viewport() const;

	};
}

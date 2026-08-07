#pragma once

#include "engine/render/screen_layout.h"
#include "engine/render/resolution_manager.h"
#include "engine/render/device_resources.h"
#include "engine/math/matt_math.h"
#include "engine/math/colour.h"
#include "SpriteBatch.h"

namespace viewport_consts
{
	constexpr float DIVIDER_THICKNESS = 2.0f;
	const std::string DIVIDER_SHEET_NAME = "sprite_sheet_1";
	const std::string DIVIDER_FRAME_NAME = "pixel";
	const mattmath::Colour DIVIDER_COLOUR = colour_consts::BLACK;
}

class ViewportManager
{
public:
	ViewportManager(ResolutionManager* resolution_manager,
		DX::DeviceResources* device_resources);

	void set_layout(ScreenLayout layout);
	ScreenLayout get_layout() const { return layout_; }

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

	mattmath::Viewport get_player_viewport(int player_num) const;

	std::vector<mattmath::Viewport> get_all_viewports() const;

	mattmath::RectangleF get_camera_adjusted_player_viewport_rect(
		int player_num, const mattmath::Camera& camera) const;

	std::vector<mattmath::RectangleF> get_viewport_dividers() const;

	D3D11_VIEWPORT get_fullscreen_d3d11_viewport() const;

private:
	ResolutionManager* resolution_manager_ = nullptr;
	DX::DeviceResources* device_resources_ = nullptr;

	ScreenLayout layout_ = ScreenLayout::one_player;

	int get_player_count_from_layout(ScreenLayout layout) const;

	D3D11_VIEWPORT calculate_d3d11_viewport(ScreenLayout layout,
		int player_num, const mattmath::Vector2F& screen_size) const;
	mattmath::Viewport calculate_viewport(ScreenLayout layout,
		int player_num, const mattmath::Vector2F& screen_size) const;

	mattmath::Viewport get_fullscreen_viewport() const;

};

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
	const MattMath::Colour DIVIDER_COLOUR = colour_consts::BLACK;
}

class ViewportManager
{
public:
	ViewportManager(ResolutionManager* resolution_manager,
		DX::DeviceResources* device_resources);

	void set_layout(screen_layout layout);
	screen_layout get_layout() const { return _layout; }

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

	MattMath::Viewport get_player_viewport(int player_num) const;

	std::vector<MattMath::Viewport> get_all_viewports() const;

	MattMath::RectangleF get_camera_adjusted_player_viewport_rect(
		int player_num, const MattMath::Camera& camera) const;

	std::vector<MattMath::RectangleF> get_viewport_dividers() const;

	D3D11_VIEWPORT get_fullscreen_d3d11_viewport() const;

private:
	ResolutionManager* _resolution_manager = nullptr;
	DX::DeviceResources* _device_resources = nullptr;

	screen_layout _layout = screen_layout::ONE_PLAYER;

	int get_player_count_from_layout(screen_layout layout) const;

	D3D11_VIEWPORT calculate_d3d11_viewport(screen_layout layout,
		int player_num, const MattMath::Vector2F& screen_size) const;
	MattMath::Viewport calculate_viewport(screen_layout layout,
		int player_num, const MattMath::Vector2F& screen_size) const;

	MattMath::Viewport get_fullscreen_viewport() const;

};

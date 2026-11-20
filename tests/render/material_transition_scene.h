#pragma once

#include "engine/render/renderer.h"

namespace render_tests
{
	// Called inside a frame. The HUD crosses both world viewports.
	inline void draw_material_transition_scene(labrador::Renderer& renderer,
		labrador::TextureHandle white, const labrador::RectangleI& white_texel,
		int width, int height, bool draw_hud = true)
	{
		using namespace labrador;
		const float half_width = static_cast<float>(width / 2);
		const float pane_height = static_cast<float>(height);
		renderer.set_view_count(2);
		for (int index = 0; index < 2; ++index)
		{
			DrawList pane = renderer.view(index);
			pane.set_viewport(Viewport(index == 0 ? 0.0f : half_width,
				0.0f, half_width, pane_height));
			pane.draw_sprite(white, white_texel,
				RectangleF(0.0f, 0.0f, half_width, pane_height),
				index == 0 ? Colour::red : Colour::blue, 0.0f,
				Vector2F::zero, SpriteFlip::none, 0.0f);
		}
		if (!draw_hud)
		{
			return;
		}
		renderer.begin_overlay();
		DrawList hud = renderer.view(2);
		hud.draw_sprite(white, white_texel,
			RectangleF(half_width - 3.0f, 5.0f, 6.0f, 3.0f),
			Colour::yellow, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	}
}

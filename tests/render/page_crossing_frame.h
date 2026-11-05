#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/sprite_geometry.h"

namespace render_tests
{
	// Sprites per vertex page: the run length GL and Vulkan split a view's
	// sprites into. D3D11's max_batch_sprites and D3D12's max_page_sprites are
	// the same number today, but both are private to their backends, so no
	// test can assert the match; a backend whose page grew past this would
	// make the frame below cross fewer pages than its name says.
	inline constexpr int vertex_page_sprites =
		labrador::RunRecorder<const void*>::max_run_sprites;

	// Two full vertex pages and a partial third. Reusing only an empty or
	// single-page frame would leave allocator and vertex-page lifetime paths
	// unexercised.
	inline constexpr int page_crossing_sprites = 2 * vertex_page_sprites + 1;

	// Sets two views and records page_crossing_sprites 2x2 sprites of
	// `texture` into each, laid out and coloured by `frame` so consecutive
	// frames differ. The caller begins, submits and ends the frame.
	inline void record_page_crossing_views(labrador::Renderer& renderer,
		labrador::TextureHandle texture, int frame)
	{
		renderer.set_view_count(2);
		for (int view = 0; view < 2; ++view)
		{
			labrador::DrawList list = renderer.view(view);
			for (int sprite = 0; sprite < page_crossing_sprites; ++sprite)
			{
				const float x = static_cast<float>((sprite + frame) % 8) * 2.0f;
				const float y = static_cast<float>((sprite / 8 + view) % 8) * 2.0f;
				list.draw_sprite(texture, labrador::RectangleI(0, 0, 2, 2),
					labrador::RectangleF(x, y, 2.0f, 2.0f),
					frame % 2 == 0 ? labrador::Colour::red : labrador::Colour::blue, 0.0f,
					labrador::Vector2F::zero, labrador::SpriteFlip::none, 0.0f);
			}
		}
	}
}

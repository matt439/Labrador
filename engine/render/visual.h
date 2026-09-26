#pragma once

#include "engine/core/game_object.h"
#include "engine/render/texture_object.h"
#include "engine/math/rectangle_rotated.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"

#include <string>

namespace labrador
{
	class Visual final : public GameObject, public TextureObject
	{
	public:
		Visual() = default;
		Visual(const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleF& rectangle,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		// The shape's angle plus rotation, about its center. Frame/caller
		// origins are additional local source-texel offsets from that center.
		Visual(const std::string& sheet_name,
			const std::string& frame_name,
			const mattmath::RectangleRotated& rect_rotated,
			RenderResources* render_resources,
			const Colour& color = Colour::white,
			float rotation = 0.0f,
			const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
			SpriteFlip flip = SpriteFlip::none,
			float layer_depth = 0.0f);

		void update(float dt) override;
		void draw(DrawList& draw_list) const override;

		// Unsnapped world geometry, including both authored and caller origin.
		// cull_bounds additionally covers view-pixel quantization at this zoom.
		// Both the geometry and per-pixel padding are cached at construction;
		// this final class exposes no geometry/frame setters. Adding one must
		// refresh both, keeping lookups and trigonometry off the cull path.
		mattmath::RectangleF bounds() const override;
		mattmath::RectangleF cull_bounds(float units_per_pixel) const override;

	protected:
		mattmath::RectangleF rectangle_ = mattmath::RectangleF::ZERO;

	private:
		mattmath::RectangleF bounds_ = mattmath::RectangleF::ZERO;
		mattmath::Vector2F pixel_padding_ = mattmath::Vector2F::ZERO;

		// From rectangle_ and the frame, once both are set.
		void initialize_bounds();
	};
}

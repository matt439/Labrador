#pragma once

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <string>

// A panel and restart message across the well when the match is over.
// PlayState registers it after ParticleField so Scene draws it over the
// top-out sparks. This separate object supplies that ordering boundary;
// layer_depth does not reorder the sample's sprites.
namespace linesweeper
{
	class TopOutBanner final : public labrador::GameObject
	{
	public:
		// Both pointers are borrowed and must outlive this object. The resource
		// table must already hold the texture/font and keep their handles valid.
		TopOutBanner(const World* world,
			labrador::RenderResources* render_resources);

		// Refreshes the cached message, layout size and glyph extent when
		// topped_out or the chosen touch wording changes. Construction primes
		// the cache; concurrent draw() calls only read it.
		void update(float dt) override;

		// Chooses touch restart wording. The next update reads this choice;
		// it does not test which input devices are available.
		void set_touch(bool touch);

		void draw(labrador::DrawList& draw_list) const override;

		// The background panel and the centered text's layout and glyph cells,
		// including bearings outside the layout rectangle.
		labrador::RectangleF bounds() const override;
		// Adds the panel's view-pixel quantization allowance at this zoom.
		labrador::RectangleF cull_bounds(float units_per_pixel) const override;

	private:
		labrador::RectangleF panel_bounds() const;

		const World* world_ = nullptr;
		labrador::RenderResources* render_resources_ = nullptr;

		labrador::TextureHandle block_;
		labrador::FontHandle font_;

		std::wstring text_;
		labrador::Vector2F text_size_;
		labrador::RectangleF text_bounds_;

		// What text_ was built from, and whether it has been built at all -
		// which a flag that starts at zero cannot say on its own.
		std::uint8_t shown_topped_out_ = 0;
		bool touch_ = false;
		bool shown_touch_ = false;
		bool built_ = false;
	};
}

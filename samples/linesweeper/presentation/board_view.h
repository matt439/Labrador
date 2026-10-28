#pragma once

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <string>

// The well, landing shadow, hold and preview panels, and HUD read one World.
// Presentation uses the values and queries in rules/world.h and rules/tables.h;
// it does not include the mutating rules/tick.h interface. The sample's
// rules/presentation/states boundary is described in samples/linesweeper/README.md.
//
// PlayState registers this object before ParticleField and TopOutBanner. Scene
// preserves that object order, so the sparks draw over the board and the banner
// over the sparks. layer_depth does not establish this ordering.
namespace linesweeper
{
	class BoardView final : public labrador::GameObject
	{
	public:
		// The World and palette are borrowed and must outlive this object.
		// The resource table must already contain the named texture and font;
		// their resolved handles must remain usable by every draw.
		//
		// `palette` is read on every draw and never written here, so the
		// state can change it between two frames and the next one shows it
		// (palette.h). It is required: nothing draws a piece without saying
		// which palette it is in.
		//
		// Asset names resolve to handles during construction, not during draw
		// (T7, T8).
		BoardView(const World* world,
			labrador::RenderResources* render_resources,
			const Palette* palette);

		// Refreshes cached score, line and level strings when their values
		// change. The constructor primes them for a draw before any update.
		// draw() only reads this cache, including concurrent view draws.
		void update(float dt) override;

		void draw(labrador::DrawList& draw_list) const override;

		// The panel's board-space extent: well, side columns and HUD. Scene
		// uses it for culling in every view, including the sample's single view.
		labrador::RectangleF bounds() const override;

	private:
		void draw_well(labrador::DrawList& draw_list) const;
		void draw_stack(labrador::DrawList& draw_list) const;
		void draw_falling(labrador::DrawList& draw_list) const;
		void draw_side_panels(labrador::DrawList& draw_list) const;
		void draw_numbers(labrador::DrawList& draw_list) const;

		// BoardView's solid rectangles are tinted sprites from one white texel.
		void fill(labrador::DrawList& draw_list,
			const labrador::RectangleF& rectangle,
			const labrador::Colour& colour) const;

		// A piece's four cells, at the well's scale or the preview's.
		void draw_piece(labrador::DrawList& draw_list, const Piece& piece,
			const labrador::Colour& colour) const;
		// Where the falling piece lands: each cell an outline in the piece's
		// colour around a faint inside (palette.h).
		void draw_shadow(labrador::DrawList& draw_list,
			const Piece& piece) const;
		void draw_preview(labrador::DrawList& draw_list, Kind kind,
			const labrador::Vector2F& origin) const;

		const World* world_ = nullptr;
		const Palette* palette_ = nullptr;

		labrador::TextureHandle block_;
		labrador::FontHandle font_;

		std::wstring score_;
		std::wstring lines_;
		std::wstring level_;

		// The cache's source values avoid formatting unchanged numbers (T8).
		std::uint32_t shown_score_ = 0;
		std::uint32_t shown_lines_ = 0;
		std::uint8_t shown_level_ = 0;

		// Initial zero values still need their first string representation.
		bool built_ = false;
	};
}

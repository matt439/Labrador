#pragma once

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <string>

// The match, drawn.
//
// THE LAYER RULE, AND THIS FILE IS THE HALF THAT PROVES IT. It includes
// rules/world.h and rules/tables.h - the value and the data - and it does not
// include rules/tick.h. There is no way to step a match from here, so "read
// the presentation without learning the rules" is a property of the include
// list rather than an aspiration (README, Three layers).
//
// It is one object and not five, because the well, the shadow, the hold slot,
// the preview and the numbers are one read of one value. Splitting them would
// buy five bounds() nobody culls against and five borrowed World pointers to
// keep in step.
namespace linesweeper
{
	class BoardView final : public artattack::GameObject
	{
	public:
		// Both borrowed. The state owns the World by value and outlives this,
		// and the resource table was filled before either existed
		// (PHILOSOPHY, Services and lifetimes).
		//
		// The handles are resolved here, once, and never looked up by name
		// again - which is the only thing T7 asks of a name.
		BoardView(const World* world,
			artattack::RenderResources* render_resources);

		// Rebuilds the four strings the HUD draws, and only when the number
		// behind one has changed.
		//
		// THIS IS WHY THE STRINGS ARE MEMBERS. draw() is const and every view
		// worker enters it on this same object at once, so a string built
		// there would be built several times over on several threads, into
		// storage none of them may write. update() runs once, on one thread,
		// and is the half of the split that is allowed to compute.
		void update(float dt) override;

		void draw(artattack::DrawList& draw_list) const override;

		// The whole panel: the well, both side columns and the numbers under
		// them. One view, so nothing ever culls this - it is reported because
		// the interface asks and because the day this sample grows a second
		// view it should already be right.
		mattmath::RectangleF bounds() const override;

	private:
		void draw_well(artattack::DrawList& draw_list) const;
		void draw_stack(artattack::DrawList& draw_list) const;
		void draw_falling(artattack::DrawList& draw_list) const;
		void draw_side_panels(artattack::DrawList& draw_list) const;
		void draw_numbers(artattack::DrawList& draw_list) const;

		// One quad. Every filled square on the screen goes through here, so
		// there is exactly one place that knows a block is a white texel with
		// a tint on it.
		void fill(artattack::DrawList& draw_list,
			const mattmath::RectangleF& rectangle,
			const artattack::Colour& colour) const;

		// A piece's four cells, at the well's scale or the preview's.
		void draw_piece(artattack::DrawList& draw_list, const Piece& piece,
			const artattack::Colour& colour) const;
		void draw_preview(artattack::DrawList& draw_list, Kind kind,
			const mattmath::Vector2F& origin) const;

		const World* world_ = nullptr;

		// Kept for measure_text, which is the font table's job and not the
		// renderer's - measuring needs the atlas, and the atlas is here
		// (render_resources.h). Read in update() only: it is not const, and
		// draw() is.
		artattack::RenderResources* render_resources_ = nullptr;

		artattack::TextureHandle block_;
		artattack::FontHandle font_;

		std::wstring score_;
		std::wstring lines_;
		std::wstring level_;
		std::wstring banner_;

		// Measured when the banner changes, which is twice a game. Measuring
		// walks the string through the font atlas, and draw() is the one place
		// that must not.
		mattmath::Vector2F banner_size_;

		// What the strings above were built from. Four comparisons a tick
		// against four allocations a frame for numbers that change a handful
		// of times a minute (T8).
		std::uint32_t shown_score_ = 0;
		std::uint32_t shown_lines_ = 0;
		std::uint8_t shown_level_ = 0;
		std::uint8_t shown_topped_out_ = 0;

		// Whether any of the four has been built yet, which a zero counter
		// cannot say on its own.
		bool built_ = false;
	};
}

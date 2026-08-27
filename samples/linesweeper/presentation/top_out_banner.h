#pragma once

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <string>

// The words across the well when the match is over, and nothing else.
//
// IT IS A SEPARATE OBJECT BECAUSE ORDER IS THIS SAMPLE'S ONLY DEPTH, and that
// is the whole reason this file exists. board_view.h argues that the well, the
// shadow, the hold slot, the preview and the numbers are one object because
// they are one read of one value, and that argument still holds for all five.
// The banner was the sixth and it is not like them: every draw in this sample
// is at layer_depth 0, so the scene draws in the order it was filled, and the
// particle field is registered between the board and this. A banner inside
// BoardView draws underneath ten thousand sparks.
//
// That is not a hypothetical. The top-out is the loudest burst the field
// throws - forty-eight particles for every filled cell of the well, nine and a
// half thousand of them - and it is thrown on exactly the frame these words
// appear. Drawn from inside BoardView they were unreadable for the first
// second of the one screen a player has to read.
//
// So the split is bought by an ordering constraint rather than by tidiness,
// which is the only thing board_view.h's argument leaves room for. It costs
// one more bounds() nobody culls against and one more borrowed World pointer -
// the two prices that header names - and it buys the property README, Additive
// blending, claims: red words over whatever colour the stack happens to be,
// legible.
namespace linesweeper
{
	class TopOutBanner final : public labrador::GameObject
	{
	public:
		// Both borrowed, on the same terms BoardView takes them: the state
		// owns the World by value and outlives this, and the resource table
		// was filled before either existed.
		TopOutBanner(const World* world,
			labrador::RenderResources* render_resources);

		// Builds and measures the string, and only when the match crosses into
		// or out of being over - twice a game.
		//
		// Measuring walks the string through the font atlas and draw() is the
		// one place that must not, because every view worker enters it on this
		// same object at once (board_view.h says the same thing about the four
		// numbers beside this).
		void update(float dt) override;

		void draw(labrador::DrawList& draw_list) const override;

		// The band the quad covers, which is a strip across the middle of the
		// well - not the well. Nothing culls it today; it is reported honestly
		// because the day this sample grows a second view it should already be
		// right.
		mattmath::RectangleF bounds() const override;

	private:
		const World* world_ = nullptr;
		labrador::RenderResources* render_resources_ = nullptr;

		labrador::TextureHandle block_;
		labrador::FontHandle font_;

		std::wstring text_;
		mattmath::Vector2F text_size_;

		// What text_ was built from, and whether it has been built at all -
		// which a flag that starts at zero cannot say on its own.
		std::uint8_t shown_topped_out_ = 0;
		bool built_ = false;
	};
}

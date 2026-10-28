#include "samples/linesweeper/presentation/top_out_banner.h"

#include "engine/math/rectanglei.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/palette.h"

#include <cmath>
#include <string>

using namespace labrador;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// Each object resolves its own asset names during construction (T7).
		const std::string block_texture_name = "white";
		const std::string font_name = "jetbrains_mono_bold_16";

		constexpr Colour banner_colour(240, 96, 96);

		// How far the quad extends past the text, top and bottom.
		constexpr float banner_padding = 10.0f;
	}

	TopOutBanner::TopOutBanner(const World* world,
		RenderResources* render_resources) :
		world_(world),
		render_resources_(render_resources),
		block_(render_resources->resolve_texture(block_texture_name)),
		font_(render_resources->resolve_sprite_font(font_name))
	{
		// Populate the cache for a first draw that precedes any scene update.
		this->update(0.0f);
	}

	void TopOutBanner::update(float /*dt*/)
	{
		const bool first = !this->built_;
		this->built_ = true;

		if (!first && this->world_->topped_out == this->shown_topped_out_ &&
			this->touch_ == this->shown_touch_)
		{
			return;
		}

		this->shown_topped_out_ = this->world_->topped_out;
		this->shown_touch_ = this->touch_;

		// A tap anywhere off the controls restarts, which play_state.cpp
		// decides; this only has to say so.
		this->text_ = this->shown_topped_out_ == 0
			? std::wstring()
			: this->shown_touch_
				? std::wstring(L"TOPPED OUT - TAP TO RESTART")
				: std::wstring(L"TOPPED OUT - PRESS R OR START");

		this->text_size_ = this->text_.empty()
			? Vector2F::zero
			: this->render_resources_->measure_text(this->font_, this->text_);
	}

	void TopOutBanner::set_touch(bool touch)
	{
		this->touch_ = touch;
	}

	void TopOutBanner::draw(DrawList& draw_list) const
	{
		if (this->text_.empty())
		{
			return;
		}

		const float middle = well_origin_y + well_height * 0.5f;

		// Premultiplied black attenuates the board and sparks behind the text
		// while leaving some background visible. The shared sprite blend state
		// handles this panel and its glyphs; samples/linesweeper/README.md,
		// Additive blending, describes the compositing choice.
		draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
			RectangleF(well_origin_x, middle - banner_padding, well_width,
				this->text_size_.y + banner_padding * 2.0f),
			faded(Colour(0.0f, 0.0f, 0.0f), 0.88f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);

		// Floor the centered board-space origin so an odd measured width does
		// not place it on a half unit at the sample's capture resolution.
		draw_list.draw_text(this->font_, this->text_,
			Vector2F(
				std::floor(
					well_origin_x + (well_width - this->text_size_.x) * 0.5f),
				middle),
			banner_colour, 1.0f, 0.0f, Vector2F::zero, 0.0f);
	}

	RectangleF TopOutBanner::bounds() const
	{
		const float middle = well_origin_y + well_height * 0.5f;

		return RectangleF(well_origin_x, middle - banner_padding, well_width,
			this->text_size_.y + banner_padding * 2.0f);
	}
}

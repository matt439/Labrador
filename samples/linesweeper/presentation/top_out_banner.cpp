#include "samples/linesweeper/presentation/top_out_banner.h"

#include "engine/math/rectanglei.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/palette.h"

#include <string>

using namespace mattmath;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// The same two assets everything else in this folder draws from. A
		// name is spelt where the object that resolves it lives, which is the
		// pattern board_view.cpp and play_state.cpp already follow.
		const std::string block_texture_name = "white";
		const std::string font_name = "courier_new_bold_16";

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
		// The first frame is a real frame and is drawn before the first
		// update(), so the string has to be right now rather than in a tick.
		this->update(0.0f);
	}

	void TopOutBanner::update(float /*dt*/)
	{
		const bool first = !this->built_;
		this->built_ = true;

		if (!first && this->world_->topped_out == this->shown_topped_out_)
		{
			return;
		}

		this->shown_topped_out_ = this->world_->topped_out;

		this->text_ = this->shown_topped_out_ != 0
			? std::wstring(L"TOPPED OUT - PRESS R OR START")
			: std::wstring();

		this->text_size_ = this->text_.empty()
			? Vector2F::ZERO
			: this->render_resources_->measure_text(this->font_, this->text_);
	}

	void TopOutBanner::draw(DrawList& draw_list) const
	{
		if (this->text_.empty())
		{
			return;
		}

		const float middle = well_origin_y + well_height * 0.5f;

		// A SOFT QUAD BEHIND THE TEXT, which is the workaround README names
		// for text this engine cannot make glow: draw_text puts its glyphs in
		// the same batch as everything else, and the tool that builds a
		// .spritefont will not write the transparent-black glyphs additive
		// blending would need. Here it earns its place twice over - red words
		// over an orange block are unreadable whatever they are blended with,
		// and since the particle field landed there are ten thousand sparks
		// over them as well.
		//
		// The quad is premultiplied black at 88%, so it does not darken to
		// black: it leaves an eighth of what is behind it showing through,
		// which is what makes it read as a banner over the well rather than a
		// hole in it. One blend state, three jobs (README, Additive blending).
		draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1),
			RectangleF(well_origin_x, middle - banner_padding, well_width,
				this->text_size_.y + banner_padding * 2.0f),
			faded(Colour(0.0f, 0.0f, 0.0f), 0.88f), 0.0f, Vector2F::ZERO,
			SpriteFlip::none, 0.0f);

		draw_list.draw_text(this->font_, this->text_,
			Vector2F(
				well_origin_x + (well_width - this->text_size_.x) * 0.5f,
				middle),
			banner_colour, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
	}

	RectangleF TopOutBanner::bounds() const
	{
		const float middle = well_origin_y + well_height * 0.5f;

		return RectangleF(well_origin_x, middle - banner_padding, well_width,
			this->text_size_.y + banner_padding * 2.0f);
	}
}

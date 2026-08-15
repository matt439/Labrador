#include "samples/linesweeper/presentation/board_view.h"

#include "engine/math/rectanglei.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/rules/tables.h"

#include <array>
#include <cstddef>
#include <string>

using namespace mattmath;
using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// Asset names, spelt once and resolved to handles once (T7, T8).
		//
		// ONE WHITE TEXEL IS THE WHOLE ATLAS. Every block, every panel and
		// every rule on this screen is that one texture with a tint on it,
		// because a solid rectangle is a sprite and this engine draws sprites
		// - there is no draw_rect on the seam and there should not be one. It
		// is 132 bytes on disk, which is what keeps README's "this sample's
		// content is a font and a JSON file" nearly true and well clear of the
		// megabytes that would be the signal to move the samples out.
		const std::string block_texture_name = "white";
		const std::string font_name = "courier_new_bold_16";

		// The layout, in back-buffer pixels at the 1280x720 main.cpp asks for.
		// Constants next to the one thing they configure, which is this file
		// (CONVENTIONS, Constants).
		constexpr float cell_size = 28.0f;
		constexpr float cell_inset = 1.0f;
		constexpr float frame_thickness = 3.0f;

		constexpr float preview_cell_size = 18.0f;
		constexpr float preview_box_width = 4.0f * preview_cell_size;
		constexpr float preview_box_height = 2.0f * preview_cell_size;
		constexpr float next_spacing = 52.0f;
		constexpr int next_shown = 5;

		constexpr float hud_line_height = 26.0f;

		constexpr float well_width =
			static_cast<float>(well_columns) * cell_size;
		constexpr float well_height =
			static_cast<float>(well_visible_rows) * cell_size;

		// Vector2F's constructor is not constexpr, so these are dynamically
		// initialised - which is safe here and worth saying why: each depends
		// on two float literals and on nothing in another translation unit, so
		// there is no initialisation order to get wrong.
		const Vector2F well_origin(500.0f, 72.0f);
		const Vector2F hold_origin(344.0f, 104.0f);
		const Vector2F next_origin(816.0f, 104.0f);
		const Vector2F label_offset(0.0f, -26.0f);
		const Vector2F hud_origin(344.0f, 236.0f);

		// Spelt out rather than taken from Colour's palette, which is a set of
		// `static const` objects in another translation unit: a namespace-scope
		// copy of one of those is a cross-unit initialisation order the
		// standard does not fix. Inside a function it would be fine; here it
		// is a black rectangle on some builds and not others.
		constexpr Colour frame_colour(60, 66, 78);
		constexpr Colour well_colour(12, 14, 18);
		constexpr Colour grid_colour = faded(Colour(1.0f, 1.0f, 1.0f), 0.05f);
		constexpr Colour label_colour(150, 158, 172);
		constexpr Colour value_colour(255, 255, 255);
		constexpr Colour banner_colour(240, 96, 96);

		// How much of a piece's own colour the landing shadow keeps.
		constexpr float shadow_alpha = 0.28f;

		// Where a well cell lands on the screen. Row well_buffer_rows is the
		// top VISIBLE row, so the two spawn rows above it map to negative
		// offsets and are never asked for - draw_stack and draw_falling both
		// start counting at the first visible row.
		RectangleF cell_rectangle(int x, int y)
		{
			return RectangleF(
				well_origin.x + static_cast<float>(x) * cell_size + cell_inset,
				well_origin.y +
					static_cast<float>(y - well_buffer_rows) * cell_size +
					cell_inset,
				cell_size - cell_inset * 2.0f,
				cell_size - cell_inset * 2.0f);
		}
	}

	BoardView::BoardView(const World* world,
		RenderResources* render_resources) :
		world_(world),
		render_resources_(render_resources),
		block_(render_resources->resolve_texture(block_texture_name)),
		font_(render_resources->resolve_sprite_font(font_name))
	{
		// The first frame is a real frame and is drawn before the first
		// update(), so the strings have to be right now rather than in a tick.
		this->update(0.0f);
	}

	void BoardView::update(float /*dt*/)
	{
		// The first pass writes all four whatever the numbers say, because
		// "unchanged" and "never built" look identical from a counter that
		// starts at zero and a match that starts at zero.
		const bool first = !this->built_;
		this->built_ = true;

		if (first || this->world_->score != this->shown_score_)
		{
			this->shown_score_ = this->world_->score;
			this->score_ = std::to_wstring(this->shown_score_);
		}

		if (first || this->world_->lines != this->shown_lines_)
		{
			this->shown_lines_ = this->world_->lines;
			this->lines_ = std::to_wstring(this->shown_lines_);
		}

		if (first || this->world_->level != this->shown_level_)
		{
			this->shown_level_ = this->world_->level;
			// Levels are stored from zero so that `World{}` is a playable
			// match, and shown from one because that is what a player counts
			// (tables.h).
			this->level_ = std::to_wstring(
				static_cast<unsigned int>(this->shown_level_) + 1u);
		}

		if (first || this->world_->topped_out != this->shown_topped_out_)
		{
			this->shown_topped_out_ = this->world_->topped_out;
			this->banner_ = this->shown_topped_out_ != 0
				? std::wstring(L"TOPPED OUT - PRESS R")
				: std::wstring();
			this->banner_size_ = this->banner_.empty()
				? Vector2F::ZERO
				: this->render_resources_->measure_text(this->font_,
					this->banner_);
		}
	}

	void BoardView::draw(DrawList& draw_list) const
	{
		this->draw_well(draw_list);
		this->draw_falling(draw_list);
		this->draw_stack(draw_list);
		this->draw_side_panels(draw_list);
		this->draw_numbers(draw_list);
	}

	RectangleF BoardView::bounds() const
	{
		return RectangleF(hold_origin.x - frame_thickness,
			well_origin.y + label_offset.y,
			next_origin.x + preview_box_width - hold_origin.x +
				frame_thickness * 2.0f,
			well_height - label_offset.y + frame_thickness);
	}

	void BoardView::fill(DrawList& draw_list, const RectangleF& rectangle,
		const Colour& colour) const
	{
		// The source is the whole texture, which is one texel. Origin is zero
		// because the rectangle overload measures the origin in source pixels
		// and there is only the one.
		draw_list.draw_sprite(this->block_, RectangleI(0, 0, 1, 1), rectangle,
			colour, 0.0f, Vector2F::ZERO, SpriteFlip::none, 0.0f);
	}

	void BoardView::draw_well(DrawList& draw_list) const
	{
		this->fill(draw_list,
			RectangleF(well_origin.x - frame_thickness,
				well_origin.y - frame_thickness,
				well_width + frame_thickness * 2.0f,
				well_height + frame_thickness * 2.0f),
			frame_colour);

		this->fill(draw_list,
			RectangleF(well_origin.x, well_origin.y, well_width, well_height),
			well_colour);

		// A grid, so an empty well still reads as ten columns. Thirty-two
		// quads, once, rather than two hundred empty cells.
		for (int column = 1; column < well_columns; ++column)
		{
			this->fill(draw_list,
				RectangleF(
					well_origin.x + static_cast<float>(column) * cell_size,
					well_origin.y, 1.0f, well_height),
				grid_colour);
		}

		for (int row = 1; row < well_visible_rows; ++row)
		{
			this->fill(draw_list,
				RectangleF(well_origin.x,
					well_origin.y + static_cast<float>(row) * cell_size,
					well_width, 1.0f),
				grid_colour);
		}
	}

	void BoardView::draw_stack(DrawList& draw_list) const
	{
		for (int y = well_buffer_rows; y < well_rows; ++y)
		{
			for (int x = 0; x < well_columns; ++x)
			{
				const Kind kind = static_cast<Kind>(this->world_->cells[
					static_cast<std::size_t>(cell_index(x, y))]);

				if (kind == Kind::none)
				{
					continue;
				}

				this->fill(draw_list, cell_rectangle(x, y),
					kind_colour(kind));
			}
		}
	}

	void BoardView::draw_falling(DrawList& draw_list) const
	{
		const Piece& current = this->world_->current;

		if (current.kind == Kind::none)
		{
			return;
		}

		// The shadow first and underneath, because a piece resting on its own
		// landing square should read as the piece and not as a haze over it.
		//
		// It is shadow() from world.h rather than a second drop loop written
		// here, which is the point of that function being a declared query:
		// the outline the player aims with and the square a hard drop lands on
		// cannot disagree, because they are one function.
		this->draw_piece(draw_list, shadow(*this->world_),
			faded(kind_colour(current.kind), shadow_alpha));

		this->draw_piece(draw_list, current, kind_colour(current.kind));
	}

	void BoardView::draw_piece(DrawList& draw_list, const Piece& piece,
		const Colour& colour) const
	{
		if (piece.kind == Kind::none)
		{
			return;
		}

		const std::array<Coord, piece_cell_count> cells = piece_cells(piece);

		for (int index = 0; index < piece_cell_count; ++index)
		{
			// A freshly dealt piece has a cell or two in the spawn rows, and
			// the spawn rows are the ones the player never sees.
			if (cells[index].y < well_buffer_rows)
			{
				continue;
			}

			this->fill(draw_list,
				cell_rectangle(cells[index].x, cells[index].y), colour);
		}
	}

	void BoardView::draw_preview(DrawList& draw_list, Kind kind,
		const Vector2F& origin) const
	{
		if (kind == Kind::none)
		{
			return;
		}

		const std::array<Coord, piece_cell_count>& cells = shape(kind, 0);

		// Centred on its own extent rather than on its box, because the boxes
		// are four wide for I, two for O and three for the rest - a preview
		// that used the box would put O and I in visibly different places.
		int min_x = cells[0].x;
		int max_x = cells[0].x;
		int min_y = cells[0].y;
		int max_y = cells[0].y;

		for (int index = 1; index < piece_cell_count; ++index)
		{
			min_x = cells[index].x < min_x ? cells[index].x : min_x;
			max_x = cells[index].x > max_x ? cells[index].x : max_x;
			min_y = cells[index].y < min_y ? cells[index].y : min_y;
			max_y = cells[index].y > max_y ? cells[index].y : max_y;
		}

		const float width =
			static_cast<float>(max_x - min_x + 1) * preview_cell_size;
		const float height =
			static_cast<float>(max_y - min_y + 1) * preview_cell_size;

		const Vector2F top_left(
			origin.x + (preview_box_width - width) * 0.5f,
			origin.y + (preview_box_height - height) * 0.5f);

		for (int index = 0; index < piece_cell_count; ++index)
		{
			this->fill(draw_list,
				RectangleF(
					top_left.x + static_cast<float>(cells[index].x - min_x) *
						preview_cell_size + cell_inset,
					top_left.y + static_cast<float>(cells[index].y - min_y) *
						preview_cell_size + cell_inset,
					preview_cell_size - cell_inset * 2.0f,
					preview_cell_size - cell_inset * 2.0f),
				kind_colour(kind));
		}
	}

	void BoardView::draw_side_panels(DrawList& draw_list) const
	{
		draw_list.draw_text(this->font_, L"HOLD", hold_origin + label_offset,
			label_colour, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
		this->draw_preview(draw_list,
			static_cast<Kind>(this->world_->hold_kind), hold_origin);

		draw_list.draw_text(this->font_, L"NEXT", next_origin + label_offset,
			label_colour, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);

		// The queue always holds at least nine, so five is never a read past
		// what has been dealt (tick.cpp, refill_queue).
		for (int index = 0; index < next_shown; ++index)
		{
			const int slot =
				(this->world_->queue_head + index) & queue_mask;

			this->draw_preview(draw_list,
				static_cast<Kind>(
					this->world_->queue[static_cast<std::size_t>(slot)]),
				Vector2F(next_origin.x,
					next_origin.y + static_cast<float>(index) * next_spacing));
		}
	}

	void BoardView::draw_numbers(DrawList& draw_list) const
	{
		const std::array<const std::wstring*, 3> values =
			{ &this->score_, &this->lines_, &this->level_ };
		const std::array<const wchar_t*, 3> names =
			{ L"SCORE", L"LINES", L"LEVEL" };

		for (std::size_t index = 0; index < values.size(); ++index)
		{
			const float top = hud_origin.y +
				static_cast<float>(index) * hud_line_height * 2.0f;

			draw_list.draw_text(this->font_, names[index],
				Vector2F(hud_origin.x, top), label_colour, 1.0f, 0.0f,
				Vector2F::ZERO, 0.0f);
			draw_list.draw_text(this->font_, *values[index],
				Vector2F(hud_origin.x, top + hud_line_height), value_colour,
				1.0f, 0.0f, Vector2F::ZERO, 0.0f);
		}

		if (this->banner_.empty())
		{
			return;
		}

		// A SOFT QUAD BEHIND THE TEXT, which is the workaround README names
		// for text this engine cannot make glow: draw_text puts its glyphs in
		// the same batch as everything else, and the tool that builds a
		// .spritefont will not write the transparent-black glyphs additive
		// blending would need. Here it earns its place twice over - red words
		// over an orange block are unreadable whatever they are blended with.
		//
		// The quad is premultiplied black at 88%, so it does not darken to
		// black: it leaves an eighth of the stack showing through, which is
		// what makes it read as a banner over the well rather than a hole in
		// it. One blend state, three jobs (README, Additive blending).
		const float middle = well_origin.y + well_height * 0.5f;
		const float padding = 10.0f;

		this->fill(draw_list,
			RectangleF(well_origin.x, middle - padding, well_width,
				this->banner_size_.y + padding * 2.0f),
			faded(Colour(0.0f, 0.0f, 0.0f), 0.88f));

		draw_list.draw_text(this->font_, this->banner_,
			Vector2F(well_origin.x + (well_width - this->banner_size_.x) *
					0.5f,
				middle),
			banner_colour, 1.0f, 0.0f, Vector2F::ZERO, 0.0f);
	}
}

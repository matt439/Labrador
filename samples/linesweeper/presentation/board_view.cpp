#include "samples/linesweeper/presentation/board_view.h"

#include "engine/math/rectanglei.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/rules/tables.h"

#include <array>
#include <cstddef>
#include <string>

using namespace labrador;

namespace linesweeper
{
	namespace
	{
		// Asset names resolve once (T7, T8). Solid rectangles use the white
		// texture with a tint; text uses the font's atlas through the same seam.
		const std::string block_texture_name = "white";
		const std::string font_name = "jetbrains_mono_bold_16";

		// The layout, in board space: the 1280x720 main.cpp asks for, which the
		// camera fits to whatever buffer there is (arrangement.h).
		// Shared well coordinates live in layout.h so blocks and particle births
		// agree. Panel-specific dimensions stay here (CONVENTIONS, Constants).
		constexpr float frame_thickness = 3.0f;

		constexpr float preview_cell_size = 18.0f;
		constexpr float preview_box_width = 4.0f * preview_cell_size;
		constexpr float preview_box_height = 2.0f * preview_cell_size;
		constexpr float next_spacing = 52.0f;
		constexpr int next_shown = 5;

		constexpr float hud_line_height = 26.0f;

		// Vector2F's two-float constructor is constexpr, and these values
		// depend only on literals and constexpr layout fields, so each is
		// constant-initialised, with no cross-unit initialisation order.
		const Vector2F well_origin(well_origin_x, well_origin_y);
		const Vector2F hold_origin(344.0f, 104.0f);
		const Vector2F next_origin(816.0f, 104.0f);
		const Vector2F label_offset(0.0f, -26.0f);
		const Vector2F hud_origin(344.0f, 236.0f);

		// Literal colours avoid initialization dependencies on Colour's
		// namespace-scope palette objects. palette.h owns well/grid colours
		// because the landing-shadow contrast is checked against them.
		constexpr Colour frame_colour(60, 66, 78);
		constexpr Colour label_colour(150, 158, 172);
		constexpr Colour value_colour(255, 255, 255);

		// Full-strength edge width in board units. Its physical pixel width
		// follows the arrangement's camera scale.
		constexpr float shadow_edge_width = 2.0f;
	}

	BoardView::BoardView(const World* world,
		RenderResources* render_resources, const Palette* palette) :
		world_(world),
		palette_(palette),
		block_(render_resources->resolve_texture(block_texture_name)),
		font_(render_resources->resolve_sprite_font(font_name))
	{
		// Populate the cache for a first draw that precedes any scene update.
		this->update(0.0f);
	}

	void BoardView::update(float /*dt*/)
	{
		// Zero-valued counters need their initial strings as well as later
		// changes; the source values alone do not distinguish an empty cache.
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
			colour, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
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

		// Grid lines show the cell boundaries without drawing an empty-cell
		// sprite at every position.
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
		// Top-out keeps cells in World but draws their burst over the dark
		// well. A bright stack beneath the sparks raises their luminance
		// changes; samples/linesweeper/README.md, The bursts against WCAG 2.3.1,
		// records the flash check. Restart replaces World and clears this flag.
		if (this->world_->topped_out != 0)
		{
			return;
		}

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
					kind_colour(kind, *this->palette_));
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
		this->draw_shadow(draw_list, shadow(*this->world_));

		this->draw_piece(draw_list, current,
			kind_colour(current.kind, *this->palette_));
	}

	void BoardView::draw_shadow(DrawList& draw_list, const Piece& piece) const
	{
		const std::array<Coord, piece_cell_count> cells = piece_cells(piece);
		const Colour edge = shadow_edge_colour(piece.kind, *this->palette_);
		const Colour inside = shadow_fill_colour(piece.kind, *this->palette_);

		for (int index = 0; index < piece_cell_count; ++index)
		{
			if (cells[index].y < well_buffer_rows)
			{
				continue;
			}

			// Four edges and the inside between them, none overlapping, so
			// the inside's fade is over the well and not over the edge.
			const RectangleF cell =
				cell_rectangle(cells[index].x, cells[index].y);
			const float inner_height = cell.height - shadow_edge_width * 2.0f;

			this->fill(draw_list,
				RectangleF(cell.x, cell.y, cell.width, shadow_edge_width),
				edge);
			this->fill(draw_list,
				RectangleF(cell.x, cell.y + cell.height - shadow_edge_width,
					cell.width, shadow_edge_width),
				edge);
			this->fill(draw_list,
				RectangleF(cell.x, cell.y + shadow_edge_width,
					shadow_edge_width, inner_height),
				edge);
			this->fill(draw_list,
				RectangleF(cell.x + cell.width - shadow_edge_width,
					cell.y + shadow_edge_width, shadow_edge_width,
					inner_height),
				edge);
			this->fill(draw_list,
				RectangleF(cell.x + shadow_edge_width,
					cell.y + shadow_edge_width,
					cell.width - shadow_edge_width * 2.0f, inner_height),
				inside);
		}
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
				kind_colour(kind, *this->palette_));
		}
	}

	void BoardView::draw_side_panels(DrawList& draw_list) const
	{
		draw_list.draw_text(this->font_, L"HOLD", hold_origin + label_offset,
			label_colour, 1.0f, 0.0f, Vector2F::zero, 0.0f);
		this->draw_preview(draw_list,
			static_cast<Kind>(this->world_->hold_kind), hold_origin);

		draw_list.draw_text(this->font_, L"NEXT", next_origin + label_offset,
			label_colour, 1.0f, 0.0f, Vector2F::zero, 0.0f);

		// take_next refills before consuming a piece, leaving enough dealt
		// entries for this preview once play starts (rules/tick.cpp).
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
				Vector2F::zero, 0.0f);
			draw_list.draw_text(this->font_, *values[index],
				Vector2F(hud_origin.x, top + hud_line_height), value_colour,
				1.0f, 0.0f, Vector2F::zero, 0.0f);
		}
	}
}

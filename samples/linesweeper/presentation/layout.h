#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "samples/linesweeper/rules/world.h"

// Where a well cell lands on the screen, and nothing else.
//
// A HEADER WITH NO TYPE IN IT, named for what it computes, which is the shape
// palette.h already claims and CONVENTIONS calls the exception rather than a
// second pattern. It exists because two files now need the same arithmetic:
// board_view.cpp draws a cell and particles.cpp throws sparks out of one, and
// the day those two disagree about where column three is, the sparks come off
// the wrong square and nothing says so.
//
// EVERYTHING HERE IS constexpr float RATHER THAN Vector2F, and that is not
// style. Vector2F's constructor is not constexpr, so a namespace-scope one in
// a header is dynamically initialised in every translation unit that includes
// it - which is an initialisation order across units that the standard does
// not fix. Two floats have no such question, and the one Vector2F a caller
// wants is one it builds inside a function.
//
// Only the WELL is here. The hold slot, the preview column, the HUD rows and
// the banner are board_view.cpp's alone, and stay there: a constant belongs
// next to the one thing it configures until a second thing needs it
// (CONVENTIONS, Constants).
namespace linesweeper
{
	// The layout, in back-buffer pixels at the 1280x720 main.cpp asks for.
	inline constexpr float cell_size = 28.0f;
	inline constexpr float cell_inset = 1.0f;

	inline constexpr float well_origin_x = 500.0f;
	inline constexpr float well_origin_y = 72.0f;

	inline constexpr float well_width =
		static_cast<float>(well_columns) * cell_size;
	inline constexpr float well_height =
		static_cast<float>(well_visible_rows) * cell_size;

	// Row well_buffer_rows is the top VISIBLE row, so the two spawn rows above
	// it map to negative offsets and are never asked for - every caller starts
	// counting at the first visible row.
	inline mattmath::RectangleF cell_rectangle(int x, int y)
	{
		return mattmath::RectangleF(
			well_origin_x + static_cast<float>(x) * cell_size + cell_inset,
			well_origin_y +
				static_cast<float>(y - well_buffer_rows) * cell_size +
				cell_inset,
			cell_size - cell_inset * 2.0f,
			cell_size - cell_inset * 2.0f);
	}

	// The middle of a cell, which is what anything thrown out of one wants.
	inline mattmath::Vector2F cell_centre(int x, int y)
	{
		return mattmath::Vector2F(
			well_origin_x + (static_cast<float>(x) + 0.5f) * cell_size,
			well_origin_y +
				(static_cast<float>(y - well_buffer_rows) + 0.5f) * cell_size);
	}
}

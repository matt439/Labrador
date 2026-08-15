#include "samples/linesweeper/rules/tables.h"

// The three questions world.h declares and presentation/ is allowed to ask.
//
// They live here rather than in tick.cpp because all three are pure reads of
// the shape table above them and none of them is a verb: the layer rule is
// that presentation/ may read a World and may not step one, and a function
// that answers "where would this land" breaks neither half of it.
namespace linesweeper
{
	std::array<Coord, piece_cell_count> piece_cells(const Piece& piece)
	{
		const std::array<Coord, piece_cell_count>& box =
			shape(piece.kind, piece.rotation);

		std::array<Coord, piece_cell_count> cells = {};

		for (int index = 0; index < piece_cell_count; ++index)
		{
			cells[index].x = static_cast<std::int8_t>(piece.x + box[index].x);
			cells[index].y = static_cast<std::int8_t>(piece.y + box[index].y);
		}

		return cells;
	}

	bool blocked(const World& world, const Piece& piece)
	{
		// Nowhere fits nothing. A kindless piece is the state between a lock
		// and the next spawn, and every caller in tick.cpp has already checked
		// for it - but the answer a stray caller gets should be the one that
		// stops it moving, not the one that lets it walk through the floor.
		if (piece.kind == Kind::none)
		{
			return true;
		}

		const std::array<Coord, piece_cell_count> cells = piece_cells(piece);

		for (int index = 0; index < piece_cell_count; ++index)
		{
			const int x = cells[index].x;
			const int y = cells[index].y;

			if (!in_well(x, y))
			{
				return true;
			}

			if (world.cells[static_cast<std::size_t>(cell_index(x, y))] !=
				cell_empty)
			{
				return true;
			}
		}

		return false;
	}

	Piece shadow(const World& world)
	{
		Piece piece = world.current;

		if (piece.kind == Kind::none)
		{
			return piece;
		}

		// Terminates because the well has a floor: twenty-two rows down, the
		// candidate leaves it and in_well() says so.
		while (true)
		{
			Piece below = piece;
			below.y = static_cast<std::int8_t>(below.y + 1);

			if (blocked(world, below))
			{
				return piece;
			}

			piece = below;
		}
	}
}

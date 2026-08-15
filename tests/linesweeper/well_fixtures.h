#pragma once

#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"

#include <cstdint>
#include <initializer_list>

// The positions these tests play from, and the four lines of scaffolding it
// takes to set one up.
//
// THIS FILE IS THE ARGUMENT, not the tests below it. A World is a value with
// public fields and no invariant a constructor has to establish, so a test
// that wants a piece resting in a notch two rows from the floor writes the
// notch and writes the piece. There is no fixture class, no builder, no
// factory and no `Game` to construct - there is nowhere to hang one, because
// the thing under test is 276 bytes of struct and a free function that steps
// it (README, The match is one value).
//
// It is also why there is no window anywhere in this target: LineSweeperRules
// links artattack_settings, which carries compiler flags and no libraries, so
// there is no renderer to fail to create.
namespace linesweeper
{
	inline void set_cell(World& world, int x, int y, Kind kind)
	{
		world.cells[static_cast<std::size_t>(cell_index(x, y))] =
			static_cast<std::uint8_t>(kind);
	}

	inline Kind cell_at(const World& world, int x, int y)
	{
		return static_cast<Kind>(
			world.cells[static_cast<std::size_t>(cell_index(x, y))]);
	}

	// Fills a row solid apart from the listed columns. Kind::l because a
	// stack has to be made of something and the colour is the presentation's
	// business.
	inline void fill_row(World& world, int y, std::initializer_list<int> holes)
	{
		for (int x = 0; x < well_columns; ++x)
		{
			bool hole = false;

			for (int column : holes)
			{
				if (column == x)
				{
					hole = true;
				}
			}

			set_cell(world, x, y, hole ? Kind::none : Kind::l);
		}
	}

	inline int filled_cells(const World& world)
	{
		int count = 0;

		for (std::size_t index = 0; index < world.cells.size(); ++index)
		{
			if (world.cells[index] != cell_empty)
			{
				++count;
			}
		}

		return count;
	}

	inline void run(World& world, int ticks, std::uint8_t input = button_none)
	{
		for (int index = 0; index < ticks; ++index)
		{
			tick(world, input);
		}
	}

	// One tick with the button down and one with it up, which is what an
	// edge-triggered verb reads as a tap.
	inline void tap(World& world, std::uint8_t button)
	{
		tick(world, button);
		tick(world, button_none);
	}
}

#include <doctest/doctest.h>

#include "samples/linesweeper/rules/tables.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"
#include "tests/linesweeper/well_fixtures.h"

#include <cstdint>

using linesweeper::Kind;
using linesweeper::Piece;
using linesweeper::World;

namespace
{
	// A well with four rows of stack and a single empty column, and a vertical
	// I standing in it. Hard dropping is then a four-row clear.
	//
	// The I's box is four wide with its cells down the third column, so a box
	// at x = -2 puts them in column zero - which `blocked` is perfectly happy
	// with, because it asks about the four cells and not about the box.
	void stack_for_a_tetris(World& world)
	{
		for (int y = 18; y <= 21; ++y)
		{
			linesweeper::fill_row(world, y, { 0 });
		}

		world.current = Piece{ Kind::i, 1, -2, 18 };
	}

	TEST_CASE("clearing rows")
	{
		World world;

		SUBCASE("a full row goes and what was above it comes down")
		{
			linesweeper::fill_row(world, 21, { 4 });
			linesweeper::set_cell(world, 0, 20, Kind::s);

			// Stem down, so the one cell that plugs the floor is underneath
			// the flat.
			world.current = Piece{ Kind::t, 2, 3, 5 };

			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.lines == 1);
			// The T plugged column four and left its flat three behind; the
			// lone S cell above rode down one row with them.
			CHECK(linesweeper::cell_at(world, 0, 21) == Kind::s);
			CHECK(linesweeper::cell_at(world, 4, 21) == Kind::t);
			CHECK(linesweeper::filled_cells(world) == 4);
		}

		SUBCASE("four at once")
		{
			stack_for_a_tetris(world);
			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.lines == 4);
			CHECK(linesweeper::filled_cells(world) == 0);
			CHECK(world.score == linesweeper::line_score[4]);
		}

		SUBCASE("ten rows is a level, and the level multiplies the score")
		{
			stack_for_a_tetris(world);
			linesweeper::tick(world, linesweeper::button_hard_drop);
			REQUIRE(world.level == 0);

			for (int round = 0; round < 2; ++round)
			{
				linesweeper::tick(world, linesweeper::button_none);
				stack_for_a_tetris(world);
				linesweeper::tick(world, linesweeper::button_hard_drop);
			}

			CHECK(world.lines == 12);
			CHECK(world.level == 1);
		}
	}

	TEST_CASE("back-to-back and combo")
	{
		World world;

		stack_for_a_tetris(world);
		linesweeper::tick(world, linesweeper::button_hard_drop);

		REQUIRE(world.score == 800);
		REQUIRE(world.back_to_back == 1);
		REQUIRE(world.combo == 1);

		linesweeper::tick(world, linesweeper::button_none);
		stack_for_a_tetris(world);
		linesweeper::tick(world, linesweeper::button_hard_drop);

		// Half as much again for the chain, plus fifty a level for the second
		// clear in a row.
		CHECK(world.score == 800 + 1200 + 50);
		CHECK(world.combo == 2);

		SUBCASE("an ordinary clear breaks the chain")
		{
			linesweeper::tick(world, linesweeper::button_none);
			linesweeper::fill_row(world, 21, { 4 });
			world.current = Piece{ Kind::t, 2, 3, 5 };
			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.lines == 9);
			CHECK(world.back_to_back == 0);
		}

		SUBCASE("dropping a piece without clearing breaks the combo")
		{
			linesweeper::tick(world, linesweeper::button_none);
			world.current = Piece{ Kind::t, 0, 3, 5 };
			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.combo == 0);
			// The chain survives, because nothing cleared to break it.
			CHECK(world.back_to_back == 1);
		}
	}

	// The setup this whole rotation system is carried for.
	//
	// Row 19 is open across columns three to five, row 20 only at column four,
	// and there is an overhang at (5, 18). A T cannot be dropped into that -
	// the overhang is in the way - so the only way in is to stand it on end
	// beside the slot and turn it.
	TEST_CASE("T-spins")
	{
		World world;

		SUBCASE("a full one, taking two rows with it")
		{
			linesweeper::fill_row(world, 19, { 3, 4, 5 });
			linesweeper::fill_row(world, 20, { 4 });
			linesweeper::set_cell(world, 5, 18, Kind::l);

			world.current = Piece{ Kind::t, 1, 3, 18 };
			REQUIRE_FALSE(linesweeper::blocked(world, world.current));

			// Nothing to kick against: the turn fits where it stands, which
			// is what makes this a spin rather than a wriggle.
			linesweeper::tick(world, linesweeper::button_rotate_clockwise);
			REQUIRE(world.current.rotation == 2);
			REQUIRE(world.last_action_rotation == 1);

			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.lines == 2);
			CHECK(world.score == linesweeper::spin_score[2]);
			CHECK(world.back_to_back == 1);
		}

		SUBCASE("a mini, when only one front corner is filled")
		{
			linesweeper::set_cell(world, 3, 18, Kind::l);
			linesweeper::set_cell(world, 5, 18, Kind::l);
			linesweeper::set_cell(world, 3, 20, Kind::l);

			world.current = Piece{ Kind::t, 1, 3, 18 };
			linesweeper::tick(world, linesweeper::button_rotate_clockwise);
			REQUIRE(world.current.rotation == 2);

			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.lines == 0);
			CHECK(world.score == linesweeper::mini_spin_score[0]);
		}

		SUBCASE("the same two rows without the turn are worth a quarter")
		{
			linesweeper::fill_row(world, 19, { 3, 4, 5 });
			linesweeper::fill_row(world, 20, { 4 });
			linesweeper::set_cell(world, 5, 18, Kind::l);

			// The identical lock, reached without a rotation. One byte of the
			// World differs and it is worth nine hundred points, which is the
			// whole reason last_action_rotation is a field rather than a
			// local.
			world.current = Piece{ Kind::t, 2, 3, 18 };
			REQUIRE_FALSE(linesweeper::blocked(world, world.current));
			REQUIRE(world.last_action_rotation == 0);

			linesweeper::tick(world, linesweeper::button_hard_drop);

			CHECK(world.lines == 2);
			CHECK(world.score == linesweeper::line_score[2]);
		}
	}

	TEST_CASE("a wall kick moves the piece the table's second offset")
	{
		World world;

		// Standing on end with its stem to the right, one column off the left
		// wall. Turning it back upright wants a cell in column minus one, so
		// the first test fails and the second - one column right - takes it.
		world.current = Piece{ Kind::t, 1, -1, 10 };
		REQUIRE_FALSE(linesweeper::blocked(world, world.current));

		linesweeper::tick(world, linesweeper::button_rotate_anticlockwise);

		CHECK(world.current.rotation == 0);
		CHECK(world.current.x == 0);
		CHECK(world.last_kick_index == 1);
	}

	TEST_CASE("a rotation with nowhere to go does not happen")
	{
		World world;

		// A pocket exactly the shape of a flat T, on the floor. Turning it
		// wants a cell below the stem and every one of the five offsets that
		// could find one is in the stack.
		linesweeper::fill_row(world, 19, { 4 });
		linesweeper::fill_row(world, 20, { 3, 4, 5 });
		linesweeper::fill_row(world, 21, {});
		world.current = Piece{ Kind::t, 0, 3, 19 };
		REQUIRE_FALSE(linesweeper::blocked(world, world.current));

		linesweeper::tick(world, linesweeper::button_rotate_clockwise);

		CHECK(world.current.rotation == 0);
		CHECK(world.current.x == 3);
		CHECK(world.current.y == 19);
		// A rotation that did not happen sets no flag, so it cannot be
		// cashed in as a T-spin.
		CHECK(world.last_action_rotation == 0);
	}
}

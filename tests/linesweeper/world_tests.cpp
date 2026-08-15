#include <doctest/doctest.h>

#include "samples/linesweeper/rules/tables.h"
#include "samples/linesweeper/rules/world.h"
#include "tests/linesweeper/well_fixtures.h"

#include <array>
#include <cstdint>

using linesweeper::Coord;
using linesweeper::Kind;
using linesweeper::Piece;
using linesweeper::World;

namespace
{
	// The two properties world.h's static_asserts cannot state, because both
	// are about two Worlds rather than one type.
	TEST_CASE("a match is a value")
	{
		SUBCASE("two fresh ones are the same match")
		{
			const World left;
			const World right;

			CHECK(linesweeper::identical(left, right));
		}

		SUBCASE("restarting is an assignment and leaves nothing behind")
		{
			World world;
			linesweeper::run(world, 400, linesweeper::button_soft_drop);

			REQUIRE(world.tick == 400);
			REQUIRE(linesweeper::filled_cells(world) > 0);

			world = World{};

			CHECK(linesweeper::identical(world, World{}));
		}

		SUBCASE("a snapshot is a copy, and one cell tells two apart")
		{
			World world;
			linesweeper::run(world, 120);

			World snapshot = world;
			CHECK(linesweeper::identical(world, snapshot));

			// Any single byte, anywhere in the value. There is no dirty flag
			// and nothing to notify, which is what makes memcmp the whole of
			// the comparison.
			linesweeper::set_cell(snapshot, 0, 0, Kind::z);
			CHECK_FALSE(linesweeper::identical(world, snapshot));
		}
	}

	TEST_CASE("a piece knows which four cells it is standing on")
	{
		SUBCASE("the box position offsets every cell")
		{
			const Piece origin{ Kind::t, 0, 0, 0 };
			const Piece moved{ Kind::t, 0, 4, 7 };

			const std::array<Coord, linesweeper::piece_cell_count> from_origin =
				linesweeper::piece_cells(origin);
			const std::array<Coord, linesweeper::piece_cell_count> from_moved =
				linesweeper::piece_cells(moved);

			for (int index = 0; index < linesweeper::piece_cell_count; ++index)
			{
				CHECK(from_moved[index].x == from_origin[index].x + 4);
				CHECK(from_moved[index].y == from_origin[index].y + 7);
			}
		}

		SUBCASE("every kind in every rotation has four distinct cells")
		{
			for (int kind = 1; kind <= linesweeper::kind_count; ++kind)
			{
				for (int rotation = 0;
					rotation < linesweeper::rotation_count; ++rotation)
				{
					const Piece piece{ static_cast<Kind>(kind),
						static_cast<std::uint8_t>(rotation), 0, 0 };
					const std::array<Coord, linesweeper::piece_cell_count>
						cells = linesweeper::piece_cells(piece);

					int distinct = 0;

					for (int left = 0;
						left < linesweeper::piece_cell_count; ++left)
					{
						bool duplicate = false;

						for (int right = 0; right < left; ++right)
						{
							if (cells[left].x == cells[right].x &&
								cells[left].y == cells[right].y)
							{
								duplicate = true;
							}
						}

						if (!duplicate)
						{
							++distinct;
						}
					}

					CHECK(distinct == linesweeper::piece_cell_count);
				}
			}
		}
	}

	TEST_CASE("the well has four walls")
	{
		World world;

		SUBCASE("the floor")
		{
			// T's box is three tall with its cells in the top two rows, so
			// row 20 is the last box row that fits.
			CHECK_FALSE(linesweeper::blocked(world, Piece{ Kind::t, 0, 3, 20 }));
			CHECK(linesweeper::blocked(world, Piece{ Kind::t, 0, 3, 21 }));
		}

		SUBCASE("the ceiling is hard, which two buffer rows make it")
		{
			CHECK_FALSE(linesweeper::blocked(world, Piece{ Kind::t, 0, 3, 0 }));
			CHECK(linesweeper::blocked(world, Piece{ Kind::t, 0, 3, -1 }));
		}

		SUBCASE("the sides")
		{
			CHECK_FALSE(linesweeper::blocked(world, Piece{ Kind::t, 0, 0, 10 }));
			CHECK(linesweeper::blocked(world, Piece{ Kind::t, 0, -1, 10 }));
			CHECK_FALSE(linesweeper::blocked(world, Piece{ Kind::t, 0, 7, 10 }));
			CHECK(linesweeper::blocked(world, Piece{ Kind::t, 0, 8, 10 }));
		}

		SUBCASE("a filled cell")
		{
			CHECK_FALSE(linesweeper::blocked(world, Piece{ Kind::t, 0, 3, 10 }));
			linesweeper::set_cell(world, 4, 11, Kind::s);
			CHECK(linesweeper::blocked(world, Piece{ Kind::t, 0, 3, 10 }));
		}

		SUBCASE("nowhere fits nothing")
		{
			CHECK(linesweeper::blocked(world, Piece{}));
		}
	}

	TEST_CASE("the shadow is where a hard drop lands")
	{
		World world;

		SUBCASE("an empty well drops to the floor")
		{
			world.current = Piece{ Kind::t, 0, 3, 5 };
			CHECK(linesweeper::shadow(world).y == 20);
		}

		SUBCASE("a stack stops it")
		{
			linesweeper::fill_row(world, 15, {});
			world.current = Piece{ Kind::t, 0, 3, 5 };

			// Its lowest cells are the box's middle row, so the box comes to
			// rest two rows above the stack.
			CHECK(linesweeper::shadow(world).y == 13);
		}

		SUBCASE("a piece already resting does not move")
		{
			world.current = Piece{ Kind::t, 0, 3, 20 };
			CHECK(linesweeper::shadow(world).y == 20);
		}

		SUBCASE("no piece, no shadow")
		{
			CHECK(linesweeper::shadow(world).kind == Kind::none);
		}
	}
}

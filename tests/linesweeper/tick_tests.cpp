#include <doctest/doctest.h>

#include "samples/linesweeper/rules/tables.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"
#include "tests/linesweeper/well_fixtures.h"

#include <array>
#include <cstdint>

using linesweeper::Kind;
using linesweeper::Piece;
using linesweeper::World;

namespace
{
	TEST_CASE("the first tick deals a piece")
	{
		World world;
		REQUIRE(world.current.kind == Kind::none);

		linesweeper::tick(world, linesweeper::button_none);

		CHECK(world.current.kind != Kind::none);
		CHECK(world.current.x == linesweeper::spawn_x);
		// Spawned in the buffer and dropped one row, so the player sees it on
		// the frame it appears.
		CHECK(world.current.y == linesweeper::spawn_y + 1);
		CHECK(world.hold_available == 1);
		CHECK(world.tick == 1);
	}

	TEST_CASE("pieces are dealt in seven-bags")
	{
		World world;
		linesweeper::tick(world, linesweeper::button_none);

		SUBCASE("the first seven are one of each")
		{
			std::array<int, linesweeper::kind_count + 1> counts = {};
			++counts[static_cast<std::size_t>(world.current.kind)];

			for (int index = 0; index < linesweeper::kind_count - 1; ++index)
			{
				const int slot =
					(world.queue_head + index) & linesweeper::queue_mask;
				++counts[world.queue[static_cast<std::size_t>(slot)]];
			}

			for (int kind = 1; kind <= linesweeper::kind_count; ++kind)
			{
				CHECK(counts[static_cast<std::size_t>(kind)] == 1);
			}
		}

		SUBCASE("so are the next seven")
		{
			std::array<int, linesweeper::kind_count + 1> counts = {};

			for (int index = linesweeper::kind_count - 1;
				index < linesweeper::kind_count * 2 - 1; ++index)
			{
				const int slot =
					(world.queue_head + index) & linesweeper::queue_mask;
				++counts[world.queue[static_cast<std::size_t>(slot)]];
			}

			for (int kind = 1; kind <= linesweeper::kind_count; ++kind)
			{
				CHECK(counts[static_cast<std::size_t>(kind)] == 1);
			}
		}

		SUBCASE("the preview is never short, however long the game runs")
		{
			int shortest = linesweeper::queue_capacity;

			// Six hundred pieces, which is eighty-odd bag refills. The well is
			// swept after each one so the run is about the queue and not about
			// how long a stack of hard drops takes to top out.
			for (int step = 0; step < 600; ++step)
			{
				linesweeper::tick(world, linesweeper::button_hard_drop);
				linesweeper::tick(world, linesweeper::button_none);
				world.cells = {};

				if (world.queue_count < shortest)
				{
					shortest = world.queue_count;
				}
			}

			CHECK(world.topped_out == 0);
			CHECK(shortest >= 5);
		}
	}

	TEST_CASE("moving sideways")
	{
		World world;
		world.current = Piece{ Kind::t, 0, 3, 5 };

		SUBCASE("a press moves one cell and then charges")
		{
			linesweeper::tick(world, linesweeper::button_left);
			CHECK(world.current.x == 2);
			CHECK(world.shift_direction == linesweeper::shift_left);

			// The charge, and nothing moves during it.
			linesweeper::run(world, linesweeper::shift_delay_ticks - 1,
				linesweeper::button_left);
			CHECK(world.current.x == 2);

			linesweeper::tick(world, linesweeper::button_left);
			CHECK(world.current.x == 1);
		}

		SUBCASE("then it repeats")
		{
			linesweeper::run(world, linesweeper::shift_delay_ticks + 1,
				linesweeper::button_right);
			CHECK(world.current.x == 5);

			linesweeper::run(world, linesweeper::shift_repeat_ticks,
				linesweeper::button_right);
			CHECK(world.current.x == 6);
		}

		SUBCASE("a tap the other way takes over from a charged direction")
		{
			// Left held long enough to be repeating.
			linesweeper::run(world, 30, linesweeper::button_left);
			REQUIRE(world.current.x == 0);

			// Still holding left, and now also pressing right. The press
			// wins, which is the whole decision apply_shift documents.
			linesweeper::tick(world,
				linesweeper::button_left | linesweeper::button_right);
			CHECK(world.current.x == 1);
			CHECK(world.shift_direction == linesweeper::shift_right);
		}

		SUBCASE("releasing the new one hands the direction back")
		{
			linesweeper::tick(world, linesweeper::button_right);
			linesweeper::tick(world,
				linesweeper::button_right | linesweeper::button_left);
			REQUIRE(world.shift_direction == linesweeper::shift_left);

			linesweeper::tick(world, linesweeper::button_right);
			CHECK(world.shift_direction == linesweeper::shift_right);
		}

		SUBCASE("a wall is a wall")
		{
			linesweeper::run(world, 200, linesweeper::button_left);
			CHECK(world.current.x == 0);

			linesweeper::run(world, 200, linesweeper::button_right);
			CHECK(world.current.x == 7);
		}
	}

	TEST_CASE("gravity is counted in ticks and sub-rows")
	{
		World world;
		world.current = Piece{ Kind::t, 0, 3, 5 };

		SUBCASE("level one is a row a second, near enough")
		{
			// 1092 sub-rows a tick against 65536 to the row is one row every
			// sixty ticks and a fraction, so sixty is not yet enough.
			linesweeper::run(world, 60);
			CHECK(world.current.y == 5);

			linesweeper::tick(world, linesweeper::button_none);
			CHECK(world.current.y == 6);
		}

		SUBCASE("soft drop is twenty times that, and pays a point a row")
		{
			linesweeper::run(world, 4, linesweeper::button_soft_drop);

			CHECK(world.current.y == 6);
			CHECK(world.score == linesweeper::soft_drop_score);
		}

		SUBCASE("the curve steepens with the level")
		{
			CHECK(linesweeper::gravity_rate(0) < linesweeper::gravity_rate(1));
			CHECK(linesweeper::gravity_rate(linesweeper::max_level) >
				linesweeper::gravity_one_row);
			// Past the table it stops climbing rather than reading off the
			// end of it.
			CHECK(linesweeper::gravity_rate(linesweeper::max_level) ==
				linesweeper::gravity_rate(200));
		}
	}

	TEST_CASE("hard drop")
	{
		World world;
		world.current = Piece{ Kind::t, 0, 3, 5 };

		linesweeper::tick(world, linesweeper::button_hard_drop);

		SUBCASE("lands on the floor and pays two a row")
		{
			// Fifteen rows from the box row it was placed on to the floor.
			CHECK(world.score == linesweeper::hard_drop_score * 15);
			CHECK(linesweeper::cell_at(world, 4, 20) == Kind::t);
			CHECK(linesweeper::cell_at(world, 3, 21) == Kind::t);
			CHECK(linesweeper::cell_at(world, 4, 21) == Kind::t);
			CHECK(linesweeper::cell_at(world, 5, 21) == Kind::t);
		}

		SUBCASE("locks at once, and the next piece arrives on the next tick")
		{
			CHECK(world.current.kind == Kind::none);

			linesweeper::tick(world, linesweeper::button_none);
			CHECK(world.current.kind != Kind::none);
			CHECK(world.current.y == linesweeper::spawn_y + 1);
		}

		SUBCASE("holding the button does not chain-drop")
		{
			const int filled = linesweeper::filled_cells(world);

			// Spawns, and the button is not an edge any more.
			linesweeper::tick(world, linesweeper::button_hard_drop);
			CHECK(linesweeper::filled_cells(world) == filled);
			CHECK(world.current.kind != Kind::none);
		}
	}

	TEST_CASE("lock delay")
	{
		World world;
		world.current = Piece{ Kind::t, 0, 3, 20 };

		SUBCASE("half a second resting, then it locks")
		{
			linesweeper::run(world, linesweeper::lock_delay_ticks - 1);
			CHECK(world.current.kind == Kind::t);
			CHECK(linesweeper::filled_cells(world) == 0);

			linesweeper::tick(world, linesweeper::button_none);
			CHECK(world.current.kind == Kind::none);
			CHECK(linesweeper::filled_cells(world) == 4);
		}

		SUBCASE("a move restarts the clock")
		{
			linesweeper::run(world, 25);
			REQUIRE(world.lock_timer == 25);

			linesweeper::tick(world, linesweeper::button_left);
			CHECK(world.current.x == 2);
			CHECK(world.lock_resets == 1);
			CHECK(world.lock_timer == 1);
		}

		SUBCASE("fifteen times, and no more")
		{
			// One tick to land, because a reset is a restart of a clock that
			// is already running - a piece that has not touched down yet has
			// nothing to restart and spends nothing finding that out.
			linesweeper::run(world, 1);

			for (int index = 0; index < linesweeper::max_lock_resets; ++index)
			{
				linesweeper::tap(world, index % 2 == 0
					? linesweeper::button_right
					: linesweeper::button_left);
			}

			REQUIRE(world.current.kind == Kind::t);
			CHECK(world.lock_resets == linesweeper::max_lock_resets);
			CHECK(world.current.x == 4);

			const std::uint8_t before = world.lock_timer;
			linesweeper::tick(world, linesweeper::button_right);

			// It moved, and it bought nothing with the move.
			CHECK(world.current.x == 5);
			CHECK(world.lock_timer == before + 1);
		}

		SUBCASE("sliding off a ledge stops the clock without spending a reset")
		{
			World ledge;
			linesweeper::fill_row(ledge, 21, { 0, 1, 2 });
			ledge.current = Piece{ Kind::t, 0, 3, 19 };

			linesweeper::run(ledge, 10);
			REQUIRE(ledge.lock_timer == 10);

			// Left, off the end of the stack, and it is falling again - for
			// long enough that the level-one curve gets to move it, which
			// takes sixty ticks all on its own.
			linesweeper::run(ledge, 61, linesweeper::button_left);

			CHECK(ledge.current.kind == Kind::t);
			CHECK(ledge.current.y > 19);
			CHECK(ledge.lock_resets < linesweeper::max_lock_resets);
		}
	}

	TEST_CASE("hold")
	{
		World world;
		linesweeper::tick(world, linesweeper::button_none);

		const Kind first = world.current.kind;
		REQUIRE(world.hold_available == 1);

		SUBCASE("an empty slot takes the piece and deals another")
		{
			linesweeper::tick(world, linesweeper::button_hold);

			CHECK(world.hold_kind == static_cast<std::uint8_t>(first));
			CHECK(world.current.kind != Kind::none);
			CHECK(world.hold_available == 0);
		}

		SUBCASE("once per piece")
		{
			linesweeper::tick(world, linesweeper::button_hold);
			const Kind second = world.current.kind;

			linesweeper::tick(world, linesweeper::button_none);
			linesweeper::tick(world, linesweeper::button_hold);

			CHECK(world.current.kind == second);
			CHECK(world.hold_kind == static_cast<std::uint8_t>(first));
		}

		SUBCASE("and the swap comes back on the next piece")
		{
			linesweeper::tick(world, linesweeper::button_hold);
			linesweeper::tick(world, linesweeper::button_none);
			linesweeper::tick(world, linesweeper::button_hard_drop);
			linesweeper::tick(world, linesweeper::button_none);

			const Kind third = world.current.kind;
			REQUIRE(world.hold_available == 1);

			linesweeper::tick(world, linesweeper::button_hold);

			// The held piece comes back and the one on screen goes in.
			CHECK(world.current.kind == first);
			CHECK(world.hold_kind == static_cast<std::uint8_t>(third));
		}
	}

	TEST_CASE("the match ends two ways")
	{
		SUBCASE("block out: the spawn square is taken")
		{
			World world;
			linesweeper::fill_row(world, 0, {});
			linesweeper::fill_row(world, 1, {});

			linesweeper::tick(world, linesweeper::button_none);

			CHECK(world.topped_out == 1);
			CHECK(world.current.kind == Kind::none);
		}

		SUBCASE("lock out: the piece comes to rest where nobody can see it")
		{
			World world;
			linesweeper::fill_row(world, 2, { 0 });
			world.current = Piece{ Kind::t, 0, 3, 0 };

			linesweeper::run(world, linesweeper::lock_delay_ticks);

			CHECK(world.topped_out == 1);
		}

		SUBCASE("and a finished match ignores everything after it")
		{
			World world;
			linesweeper::fill_row(world, 0, {});
			linesweeper::fill_row(world, 1, {});
			linesweeper::tick(world, linesweeper::button_none);
			REQUIRE(world.topped_out == 1);

			const World finished = world;
			linesweeper::run(world, 100, linesweeper::button_hard_drop);

			CHECK(linesweeper::identical(world, finished));
		}
	}
}

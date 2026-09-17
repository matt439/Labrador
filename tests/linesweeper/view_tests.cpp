#include <doctest/doctest.h>

#include "engine/render/renderer.h"
#include "samples/linesweeper/presentation/layout.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cstddef>
#include <cstdint>

using labrador::TextureHandle;
using linesweeper::Coord;
using linesweeper::Kind;
using linesweeper::ParticleField;
using linesweeper::Piece;
using linesweeper::TickResult;
using linesweeper::World;
using linesweeper::cell_index;
using linesweeper::particle_capacity;
using linesweeper::piece_cell_count;
using linesweeper::piece_cells;
using linesweeper::shadow;
using linesweeper::well_columns;
using linesweeper::well_rows;

// The particle field, stepped with no device.
//
// THIS IS THE ONLY TEST IN THE TREE THAT ASSERTS ON THE SAMPLE'S PRESENTATION,
// and it is worth saying why the other four hundred lines of presentation/
// have none. board_view.cpp measures text, and measuring walks a font atlas
// that a device filled - so a BoardView cannot be constructed without one, and
// what it draws is verified by looking at it (README, Still open). The
// particle field takes a resolved handle instead of the resource table and
// nothing in update() touches it, so the whole simulation half runs here.
//
// WHAT IS ASSERTED IS THE POLICY, NOT THE TUNING. Every count below is a ratio
// or a bound: how many sparks a lock is worth against a clear is a number in
// particles.cpp's anonymous namespace and is meant to be changed by somebody
// looking at the screen. What must not change quietly is that a lock emits in
// proportion to the cells that filled, that a row leaving is louder per cell
// than a piece landing, that the field is bounded, and that it says how many
// emissions it refused.
//
// The handle is default-constructed and therefore unresolved. Reading through
// one is a throw (core/handle.h), which is exactly right: a test that
// accidentally called draw() would fail loudly rather than pass having drawn
// nothing.
namespace
{
	constexpr float frame = 1.0f / 60.0f;

	// A cell of the well, filled with piece kind 1. Which kind is immaterial
	// here - the rules store a byte and the palette colours it (world.h).
	void fill(World& world, int x, int y)
	{
		world.cells[static_cast<std::size_t>(cell_index(x, y))] = 1;
	}

	void fill_row(World& world, int y)
	{
		for (int x = 0; x < well_columns; ++x)
		{
			fill(world, x, y);
		}
	}

	void clear_row(World& world, int y)
	{
		for (int x = 0; x < well_columns; ++x)
		{
			world.cells[static_cast<std::size_t>(cell_index(x, y))] = 0;
		}
	}

	// How many particles `cells` cells appearing at once is worth. A piece
	// locking is this with four.
	int locked(int cells)
	{
		World world;
		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		for (int index = 0; index < cells; ++index)
		{
			fill(world, index, well_rows - 1);
		}

		++world.tick;
		field.update(frame);

		return field.live();
	}

	// A match one tick away from a single-row clear: the bottom row filled
	// except for two columns, and an O piece above the gap.
	//
	// The setup is checked rather than assumed - the assertions at the end are
	// the test's own proof that shadow() puts the piece where this comment
	// says it does, which is the property board_at_lock in particles.cpp rests
	// on.
	World about_to_clear()
	{
		World world;

		for (int x = 0; x < well_columns - 2; ++x)
		{
			fill(world, x, well_rows - 1);
		}

		world.current = Piece{ Kind::o, 0,
			static_cast<std::int8_t>(well_columns - 2), 0 };

		return world;
	}

	// What tick() does after the lock, done by hand: the full rows go and
	// everything above them drops.
	void clear_full_rows(World& world)
	{
		int write = well_rows - 1;

		for (int read = well_rows - 1; read >= 0; --read)
		{
			bool full = true;

			for (int x = 0; x < well_columns; ++x)
			{
				if (world.cells[static_cast<std::size_t>(
					cell_index(x, read))] == 0)
				{
					full = false;
					break;
				}
			}

			if (full)
			{
				++world.lines;
				continue;
			}

			for (int x = 0; x < well_columns; ++x)
			{
				world.cells[static_cast<std::size_t>(cell_index(x, write))] =
					world.cells[static_cast<std::size_t>(cell_index(x, read))];
			}

			--write;
		}

		for (int y = write; y >= 0; --y)
		{
			clear_row(world, y);
		}
	}

	// Locks the falling piece where a hard drop would put it, then clears -
	// which is what one tick of tick.cpp does, in the one order that matters
	// here: both, atomically, so no World ever holds a full row. And says
	// which piece it locked, which is what tick() says too.
	void lock_and_clear(World& world, TickResult& result)
	{
		const Piece landed = shadow(world);
		result.locked = landed;
		const std::array<Coord, piece_cell_count> occupied =
			piece_cells(landed);

		for (int index = 0; index < piece_cell_count; ++index)
		{
			world.cells[static_cast<std::size_t>(
				cell_index(occupied[index].x, occupied[index].y))] =
				static_cast<std::uint8_t>(landed.kind);
		}

		clear_full_rows(world);

		world.current = Piece{ Kind::t, 0, 4, 0 };
		++world.tick;
	}

	TEST_CASE("a field starts empty and stays empty until something happens")
	{
		World world;
		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		CHECK(field.live() == 0);
		CHECK(field.dropped() == 0);

		SUBCASE("a tick in which nothing changed emits nothing")
		{
			++world.tick;
			field.update(frame);

			CHECK(field.live() == 0);
		}

		SUBCASE("and a frame with no tick at all emits nothing")
		{
			field.update(frame);

			CHECK(field.live() == 0);
		}
	}

	// The field is handed no events. It works out what happened by keeping
	// last frame's match and comparing, which is the whole reason a 276-byte
	// trivially copyable World is worth having.
	TEST_CASE("a piece landing emits in proportion to the cells it left")
	{
		const int one = locked(1);
		const int four = locked(4);

		REQUIRE(one > 0);
		CHECK(four == one * 4);
	}

	// THE TICK THAT CLEARS IS THE ONE THAT LOCKS, so a full row is never
	// visible from out here and the field reconstructs the board between the
	// two. These are the cases that pin that reconstruction.
	TEST_CASE("a cleared row is found even though no World ever holds one")
	{
		World world = about_to_clear();

		// The setup's own proof: the piece drops into the gap and completes
		// the bottom row, which is what board_at_lock has to rediscover.
		const Piece landed = shadow(world);
		REQUIRE(landed.kind == Kind::o);

		// Constructed before the tick, so previous_ is the pre-lock match -
		// exactly what the field sees on a real frame.
		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});
		REQUIRE(field.live() == 0);

		lock_and_clear(world, result);
		REQUIRE(world.lines == 1);

		// And no row is full now, which is the whole difficulty.
		for (int y = 0; y < well_rows; ++y)
		{
			bool full = true;

			for (int x = 0; x < well_columns; ++x)
			{
				full = full && world.cells[static_cast<std::size_t>(
					cell_index(x, y))] != 0;
			}

			REQUIRE_FALSE(full);
		}

		field.update(frame);

		const int cleared = field.live();

		REQUIRE(cleared > 0);

		SUBCASE("one row went, so ten cells' worth of sparks came out")
		{
			CHECK(cleared % well_columns == 0);
		}

		SUBCASE("and a row leaving is louder per cell than a piece landing")
		{
			CHECK(cleared / well_columns > locked(1));
		}
	}

	// How many sparks one cleared row is worth, by the hand-rolled path the
	// case above proves.
	int one_row_cleared()
	{
		World world = about_to_clear();
		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});
		lock_and_clear(world, result);
		field.update(0.0f);
		return field.live();
	}

	// THE TICK THAT ROTATES AND LOCKS, through the real tick(). The field
	// used to rebuild the board from where a hard drop would have put LAST
	// frame's piece, and README said the failure on a piece that turned on
	// its locking tick was a missed burst. It was a burst from the wrong row
	// (docs/review/gpt6/README.md, G6-11): this is the review's board, and
	// the sparks have to come out of the row that went.
	TEST_CASE("a piece that turns on the tick it locks sparks from the row that went")
	{
		World world;

		// A horizontal I above a stack shaped so that the horizontal piece
		// would complete row 20 and the vertical one completes row 21: row
		// 20 is full except for columns 3..6, row 21 full except column 5.
		world.current = Piece{ Kind::i, 0, 3, 5 };
		for (int x = 0; x < well_columns; ++x)
		{
			if (x < 3 || x > 6)
			{
				fill(world, x, 20);
			}
			if (x != 5)
			{
				fill(world, x, 21);
			}
		}

		// The setup's own proof, from the position the field will read: the
		// horizontal shadow lands across row 20, completing it, which is the
		// row the old answer threw its sparks from.
		const Piece horizontal = shadow(world);
		REQUIRE(horizontal.rotation == 0);
		for (const Coord& cell : piece_cells(horizontal))
		{
			REQUIRE(cell.y == 20);
		}

		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		// Rotate clockwise and hard drop in one tick, which tick.cpp applies
		// in that order: the vertical I drops down column 5 into row 21.
		result = linesweeper::tick(world, static_cast<std::uint8_t>(
			linesweeper::button_rotate_clockwise |
			linesweeper::button_hard_drop));
		REQUIRE(world.lines == 1);
		REQUIRE(result.locked.kind == Kind::i);
		REQUIRE(result.locked.rotation == 1);

		bool reached_the_floor = false;
		for (const Coord& cell : piece_cells(result.locked))
		{
			reached_the_floor = reached_the_floor || (cell.x == 5 && cell.y == 21);
		}
		REQUIRE(reached_the_floor);

		// dt of zero, so the sparks sit where they were thrown from.
		field.update(0.0f);
		REQUIRE(field.live() > 0);

		// Every spark is inside row 21's band of the screen. Row 20's band
		// is the 28 pixels above it, and the review measured the old answer
		// at y = 576..604 - that band exactly.
		const float row_top = linesweeper::well_origin_y +
			static_cast<float>(21 - linesweeper::well_buffer_rows) *
			linesweeper::cell_size;
		CHECK(field.bounds().top() >= row_top - 0.01f);
		CHECK(field.bounds().bottom() <= row_top + linesweeper::cell_size + 0.01f);

		// And it is one row's worth: the same count the single-row clear
		// above produces, not that plus a lock and not two rows.
		CHECK(field.live() % well_columns == 0);
		CHECK(field.live() == one_row_cleared());
	}

	TEST_CASE("a lock that clears nothing is read as a lock")
	{
		World world;
		world.current = Piece{ Kind::o, 0, 4, 0 };

		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		lock_and_clear(world, result);
		REQUIRE(world.lines == 0);

		field.update(frame);

		// Four cells appeared and nothing was cleared, so it is worth exactly
		// what four cells appearing is worth.
		CHECK(field.live() == locked(4));
	}

	TEST_CASE("a top-out shatters the cells that are there and no others")
	{
		SUBCASE("an empty well tops out with nothing to shatter")
		{
			World world;
			TickResult result;
			ParticleField field(&world, &result, TextureHandle{});

			world.topped_out = 1;
			++world.tick;
			field.update(frame);

			CHECK(field.live() == 0);
		}

		SUBCASE("a full row is worth more per cell than a clear is")
		{
			World world;
			fill_row(world, well_rows - 1);

			TickResult result;
			ParticleField field(&world, &result, TextureHandle{});

			world.topped_out = 1;
			++world.tick;
			field.update(frame);

			CHECK(field.live() > 0);
			CHECK(field.live() % well_columns == 0);
		}

		SUBCASE("and the whole well is what the capacity was sized for")
		{
			World world;

			for (int y = 0; y < well_rows; ++y)
			{
				fill_row(world, y);
			}

			TickResult result;
			ParticleField field(&world, &result, TextureHandle{});

			world.topped_out = 1;
			++world.tick;
			field.update(frame);

			// Two hundred visible cells, and it fits with room over. The two
			// buffer rows are never drawn and never shatter.
			CHECK(field.live() > 0);
			CHECK(field.live() <= particle_capacity);
			CHECK(field.dropped() == 0);
		}
	}

	TEST_CASE("particles die, and the field empties itself")
	{
		World world;
		fill_row(world, well_rows - 1);

		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		world.topped_out = 1;
		++world.tick;
		field.update(frame);

		REQUIRE(field.live() > 0);

		// Three seconds, comfortably past the longest lifetime any emitter in
		// particles.cpp asks for.
		for (int step = 0; step < 180; ++step)
		{
			field.update(frame);
		}

		CHECK(field.live() == 0);

		// And the extent collapses with them, rather than staying wherever the
		// last burst reached.
		CHECK(field.bounds().width == doctest::Approx(0.0f));
		CHECK(field.bounds().height == doctest::Approx(0.0f));
	}

	TEST_CASE("a restart empties the field on the frame it happens")
	{
		World world;
		world.tick = 500;
		fill_row(world, well_rows - 1);

		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		world.topped_out = 1;
		++world.tick;
		field.update(frame);

		REQUIRE(field.live() > 0);

		// The one line play_state.cpp restarts with. The tick ordinal goes
		// backwards, which is the whole of how the field knows.
		world = World{};
		field.update(frame);

		CHECK(field.live() == 0);
	}

	// The documented policy, asserted rather than hoped for: a full field
	// refuses the newest emission and counts it.
	TEST_CASE("the field is bounded, and says how many it refused")
	{
		World world;
		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		// dt of zero, so nothing ages and every burst accumulates. Sixty
		// rounds of filling four rows and clearing them is far past the
		// capacity, whatever the tuning.
		for (int round = 0; round < 60; ++round)
		{
			for (int y = well_rows - 4; y < well_rows; ++y)
			{
				fill_row(world, y);
			}

			++world.tick;
			field.update(0.0f);

			for (int y = well_rows - 4; y < well_rows; ++y)
			{
				clear_row(world, y);
			}

			world.lines += 4;
			++world.tick;
			field.update(0.0f);
		}

		CHECK(field.live() == particle_capacity);
		CHECK(field.dropped() > 0);
	}

	TEST_CASE("bounds is the burst, measured, and not the whole screen")
	{
		World world;
		fill_row(world, well_rows - 1);

		TickResult result;
		ParticleField field(&world, &result, TextureHandle{});

		SUBCASE("an empty field has no area")
		{
			CHECK(field.bounds().width == doctest::Approx(0.0f));
			CHECK(field.bounds().height == doctest::Approx(0.0f));
		}

		SUBCASE("a live one covers what it threw")
		{
			world.topped_out = 1;
			++world.tick;
			field.update(frame);

			REQUIRE(field.live() > 0);
			CHECK(field.bounds().width > 0.0f);
			CHECK(field.bounds().height > 0.0f);
		}
	}
}

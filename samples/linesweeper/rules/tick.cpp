#include "samples/linesweeper/rules/tick.h"

#include "samples/linesweeper/rules/tables.h"

#include <array>
#include <cstddef>
#include <cstdint>

// The simulation. It includes two headers, both of them its own, and that is
// the whole of what the rules layer can reach.
namespace linesweeper
{
	namespace
	{
		// --- What the player is saying ---

		bool held(const World& world, std::uint8_t button)
		{
			return (world.input & button) != 0;
		}

		// Down this tick, up last tick. The same edge every input device in
		// engine/input/ reports, arrived at the same way, because an edge is
		// "down now, up before" whichever side of the wall it is computed on.
		bool pressed(const World& world, std::uint8_t button)
		{
			return (world.input & button) != 0 &&
				(world.previous_input & button) == 0;
		}

		// --- The random stream ---
		//
		// SplitMix32: the state is a plain counter, and the bits come from
		// mixing it. That choice is not about quality, it is about what
		// World::rng means - a POSITION in a sequence rather than an opaque
		// register. Restoring a snapshot restores the draw that was coming
		// next, and `World{}` is a legal starting point because zero is a
		// perfectly good position. An xorshift, which is the obvious thing to
		// reach for, is stuck at zero forever from a zero seed, and a game
		// whose default value cannot deal a piece is not the value semantics
		// this sample is arguing for.
		//
		// Seeding a different match is `world.rng = whatever;` before the
		// first tick. Nothing in the rules reaches for a clock, because a
		// clock is exactly the thing a replay cannot have.
		std::uint32_t next_random(World& world)
		{
			world.rng += 0x9E3779B9u;

			std::uint32_t value = world.rng;
			value = (value ^ (value >> 16)) * 0x21F0AAADu;
			value = (value ^ (value >> 15)) * 0x735A2D97u;

			return value ^ (value >> 15);
		}

		// --- The bag ---

		void push_queue(World& world, std::uint8_t kind)
		{
			const int slot =
				(world.queue_head + world.queue_count) & queue_mask;

			world.queue[static_cast<std::size_t>(slot)] = kind;
			++world.queue_count;
		}

		// Deals seven-piece bags until another one would not fit, which keeps
		// between nine and sixteen pieces resident. The preview shows five, so
		// it is never looking at pieces that have not been dealt.
		void refill_queue(World& world)
		{
			while (world.queue_count + kind_count <= queue_capacity)
			{
				std::array<std::uint8_t, kind_count> bag = {};

				for (int index = 0; index < kind_count; ++index)
				{
					bag[static_cast<std::size_t>(index)] =
						static_cast<std::uint8_t>(index + 1);
				}

				// Fisher-Yates, backwards, which is the version that needs one
				// draw per element and no rejection. The modulo is biased, by
				// about one part in six hundred million against a range of
				// seven; saying so is cheaper than the rejection loop that
				// would remove it.
				for (int index = kind_count - 1; index > 0; --index)
				{
					const std::uint32_t range =
						static_cast<std::uint32_t>(index + 1);
					const std::size_t target =
						static_cast<std::size_t>(next_random(world) % range);
					const std::size_t source =
						static_cast<std::size_t>(index);

					const std::uint8_t moved = bag[source];
					bag[source] = bag[target];
					bag[target] = moved;
				}

				for (int index = 0; index < kind_count; ++index)
				{
					push_queue(world, bag[static_cast<std::size_t>(index)]);
				}
			}
		}

		Kind take_next(World& world)
		{
			refill_queue(world);

			const std::uint8_t kind = world.queue[world.queue_head];

			world.queue_head =
				static_cast<std::uint8_t>((world.queue_head + 1) & queue_mask);
			--world.queue_count;

			return static_cast<Kind>(kind);
		}

		// --- Putting a piece in the well ---

		void reset_piece_state(World& world)
		{
			world.gravity_sub_row = 0;
			world.lock_timer = 0;
			world.lock_resets = 0;
			world.last_action_rotation = 0;
			world.last_kick_index = 0;
		}

		// A fresh piece of this kind at the spawn square, dropped one row if
		// the row is free - which is the guideline's rule and the reason the
		// player sees the piece on the frame it appears rather than a tick
		// later. False means it did not fit at all, which is the block-out.
		bool place(World& world, Kind kind)
		{
			Piece piece;
			piece.kind = kind;
			piece.rotation = 0;
			piece.x = spawn_x;
			piece.y = spawn_y;

			if (blocked(world, piece))
			{
				return false;
			}

			Piece dropped = piece;
			dropped.y = static_cast<std::int8_t>(dropped.y + 1);

			if (!blocked(world, dropped))
			{
				piece = dropped;
			}

			world.current = piece;
			reset_piece_state(world);

			return true;
		}

		void spawn(World& world)
		{
			if (!place(world, take_next(world)))
			{
				world.topped_out = 1;
				return;
			}

			// One hold per piece, and this is the piece. place() deliberately
			// does not do this, because the swap below places a piece too and
			// must not hand the player a second go.
			world.hold_available = 1;
		}

		// --- Lock delay ---

		// A move or a turn that lands while the piece is already resting
		// restarts the clock, fifteen times and no more. lock_timer is zero
		// for a piece in the air, so the test below is also the test for "was
		// there a clock to restart" - and a piece that lands later this tick
		// has not spent a reset getting there.
		void grant_lock_reset(World& world)
		{
			if (world.lock_timer == 0 || world.lock_resets >= max_lock_resets)
			{
				return;
			}

			++world.lock_resets;
			world.lock_timer = 0;
		}

		// --- Moving ---

		bool try_move(World& world, int dx)
		{
			Piece candidate = world.current;
			candidate.x = static_cast<std::int8_t>(candidate.x + dx);

			if (blocked(world, candidate))
			{
				return false;
			}

			world.current = candidate;

			// A sideways step is not a rotation, so whatever T-spin the last
			// turn set up is off. This flag and last_kick_index are the only
			// state in the simulation that one verb writes and another reads.
			world.last_action_rotation = 0;
			grant_lock_reset(world);

			return true;
		}

		void apply_hold(World& world)
		{
			if (!pressed(world, button_hold) || world.hold_available == 0)
			{
				return;
			}

			const std::uint8_t swapped = world.hold_kind;
			world.hold_kind = static_cast<std::uint8_t>(world.current.kind);

			const Kind incoming = swapped == 0
				? take_next(world)
				: static_cast<Kind>(swapped);

			if (!place(world, incoming))
			{
				world.topped_out = 1;
				return;
			}

			world.hold_available = 0;
		}

		void apply_rotation(World& world)
		{
			int direction = 0;

			if (pressed(world, button_rotate_clockwise))
			{
				direction = 1;
			}
			else if (pressed(world, button_rotate_anticlockwise))
			{
				direction = -1;
			}

			if (direction == 0)
			{
				return;
			}

			const int from = world.current.rotation;
			// Anticlockwise is three clockwise, which is one turn of
			// arithmetic instead of a second table.
			const int to = (from + (direction > 0 ? 1 : 3)) & 3;

			const KickTable& table = kicks(world.current.kind);
			const std::size_t row =
				static_cast<std::size_t>(kick_row(from, direction > 0));

			for (int test = 0; test < kick_test_count; ++test)
			{
				const Coord offset = table[row][static_cast<std::size_t>(test)];

				Piece candidate = world.current;
				candidate.rotation = static_cast<std::uint8_t>(to);
				candidate.x = static_cast<std::int8_t>(candidate.x + offset.x);
				candidate.y = static_cast<std::int8_t>(candidate.y + offset.y);

				if (blocked(world, candidate))
				{
					continue;
				}

				world.current = candidate;
				world.last_action_rotation = 1;
				world.last_kick_index = static_cast<std::uint8_t>(test);
				grant_lock_reset(world);
				return;
			}

			// All five tests failed, so the piece does not turn and nothing
			// about it changes - including the T-spin flag, which a failed
			// rotation has no business setting.
		}

		// Charge and repeat, in ticks.
		//
		// A PRESS ALWAYS TAKES OVER FROM A HOLD, which is the one part of this
		// that is a decision rather than a timer. Tapping right while left is
		// held is how a player corrects a piece mid-slide, and the simpler
		// rule - both down means neither - stops the piece dead exactly when
		// they are trying hardest to place it. The cost is the six lines
		// below that hand the direction back when a button comes up.
		void apply_shift(World& world)
		{
			const bool left_held = held(world, button_left);
			const bool right_held = held(world, button_right);

			std::uint8_t direction = world.shift_direction;

			if (pressed(world, button_left))
			{
				direction = shift_left;
			}
			if (pressed(world, button_right))
			{
				direction = shift_right;
			}

			if (direction == shift_left && !left_held)
			{
				direction = right_held ? shift_right : shift_none;
			}
			else if (direction == shift_right && !right_held)
			{
				direction = left_held ? shift_left : shift_none;
			}
			else if (direction == shift_none)
			{
				direction = left_held
					? shift_left
					: (right_held ? shift_right : shift_none);
			}

			if (direction == shift_none)
			{
				world.shift_direction = shift_none;
				world.shift_timer = 0;
				return;
			}

			const int dx = direction == shift_left ? -1 : 1;

			if (direction != world.shift_direction)
			{
				// The tap: one cell now, then the charge before it repeats.
				world.shift_direction = direction;
				world.shift_timer = shift_delay_ticks;
				try_move(world, dx);
				return;
			}

			if (world.shift_timer > 0)
			{
				--world.shift_timer;
			}

			if (world.shift_timer == 0)
			{
				world.shift_timer = shift_repeat_ticks;
				try_move(world, dx);
			}
		}

		// --- Locking ---

		bool corner_filled(const World& world, int x, int y)
		{
			return !in_well(x, y) ||
				world.cells[static_cast<std::size_t>(cell_index(x, y))] !=
					cell_empty;
		}

		// The three-corner rule, which is what everyone who plays this genre
		// means by a T-spin: a T that arrived by rotating, with three of the
		// four corners of its box occupied. Both corners the stem points
		// between makes it a full one; only one of them makes it a mini,
		// unless the turn needed the last kick in the table - which is an
		// offset no sequence of moves could have walked, so it is the one that
		// proves the piece was screwed into the slot.
		//
		// The four corners are never the piece's own cells in any of its four
		// states, so this reads the same answer before and after the stamp. It
		// is called before, because scoring the lock is easier to follow when
		// nothing has changed yet.
		Spin detect_spin(const World& world)
		{
			if (world.current.kind != Kind::t ||
				world.last_action_rotation == 0)
			{
				return Spin::none;
			}

			constexpr std::array<Coord, 4> corners =
				{{ {0, 0}, {2, 0}, {0, 2}, {2, 2} }};

			// Which two of the four the stem points between, per rotation:
			// up, right, down, left.
			constexpr std::array<std::array<int, 2>, rotation_count> front =
				{{ {{0, 1}}, {{1, 3}}, {{2, 3}}, {{0, 2}} }};

			std::array<bool, 4> filled = {};
			int filled_count = 0;

			for (std::size_t index = 0; index < corners.size(); ++index)
			{
				filled[index] = corner_filled(world,
					world.current.x + corners[index].x,
					world.current.y + corners[index].y);

				if (filled[index])
				{
					++filled_count;
				}
			}

			if (filled_count < 3)
			{
				return Spin::none;
			}

			const std::array<int, 2>& pair = front[world.current.rotation];

			if (filled[static_cast<std::size_t>(pair[0])] &&
				filled[static_cast<std::size_t>(pair[1])])
			{
				return Spin::full;
			}

			return world.last_kick_index == kick_test_count - 1
				? Spin::full
				: Spin::mini;
		}

		bool row_full(const World& world, int y)
		{
			for (int x = 0; x < well_columns; ++x)
			{
				if (world.cells[static_cast<std::size_t>(cell_index(x, y))] ==
					cell_empty)
				{
					return false;
				}
			}

			return true;
		}

		// Compacts the well upward in one pass: a read cursor and a write
		// cursor walking from the floor, the write cursor stalling on every
		// full row. What is left above the write cursor at the end is however
		// many rows went, and they are cleared rather than left as the ghost
		// of what was copied out of them.
		int clear_lines(World& world)
		{
			int write = well_rows - 1;
			int cleared = 0;

			for (int read = well_rows - 1; read >= 0; --read)
			{
				if (row_full(world, read))
				{
					++cleared;
					continue;
				}

				if (write != read)
				{
					for (int x = 0; x < well_columns; ++x)
					{
						world.cells[
							static_cast<std::size_t>(cell_index(x, write))] =
							world.cells[
								static_cast<std::size_t>(cell_index(x, read))];
					}
				}

				--write;
			}

			for (; write >= 0; --write)
			{
				for (int x = 0; x < well_columns; ++x)
				{
					world.cells[
						static_cast<std::size_t>(cell_index(x, write))] =
						cell_empty;
				}
			}

			return cleared;
		}

		void apply_score(World& world, int cleared, Spin spin)
		{
			// Levels are stored from zero so that `World{}` is a playable
			// match, and scored from one so that the first level is worth
			// something (tables.h).
			const std::uint32_t level =
				static_cast<std::uint32_t>(world.level) + 1;
			const std::size_t rows = static_cast<std::size_t>(cleared);

			std::uint32_t base = 0;

			switch (spin)
			{
			case Spin::mini:
				base = mini_spin_score[rows];
				break;
			case Spin::full:
				base = spin_score[rows];
				break;
			case Spin::none:
			default:
				base = line_score[rows];
				break;
			}

			const bool difficult = cleared == difficult_clear_rows ||
				(spin != Spin::none && cleared > 0);

			if (difficult && world.back_to_back != 0)
			{
				base = base * back_to_back_numerator /
					back_to_back_denominator;
			}

			world.score += base * level;

			if (cleared == 0)
			{
				// A T-spin that took no rows keeps a back-to-back alive - it
				// is not a clear, so it cannot break the chain - but it does
				// end a combo, because a combo counts clears.
				world.combo = 0;
				return;
			}

			world.score += combo_score *
				static_cast<std::uint32_t>(world.combo) * level;

			if (world.combo < 255)
			{
				++world.combo;
			}

			world.back_to_back = static_cast<std::uint8_t>(difficult ? 1 : 0);

			world.lines += static_cast<std::uint32_t>(cleared);

			const std::uint32_t reached = world.lines / lines_per_level;
			world.level = static_cast<std::uint8_t>(
				reached < max_level ? reached : max_level);
		}

		void lock_piece(World& world)
		{
			const Spin spin = detect_spin(world);
			const std::array<Coord, piece_cell_count> cells =
				piece_cells(world.current);

			// LOCK OUT, which is the other way to lose and the one nobody
			// implements by accident: a piece that came to rest entirely in
			// the two rows above the visible well ends the match, whatever the
			// spawn square looked like when it was dealt.
			bool wholly_hidden = true;

			for (int index = 0; index < piece_cell_count; ++index)
			{
				const int x = cells[index].x;
				const int y = cells[index].y;

				world.cells[static_cast<std::size_t>(cell_index(x, y))] =
					static_cast<std::uint8_t>(world.current.kind);

				if (y >= well_buffer_rows)
				{
					wholly_hidden = false;
				}
			}

			apply_score(world, clear_lines(world), spin);

			// No piece until the next tick, which is where the next one is
			// dealt. There is no separate spawn delay: at sixty ticks a second
			// one frame of it is what a player reads as immediate.
			world.current = Piece{};

			if (wholly_hidden)
			{
				world.topped_out = 1;
			}
		}

		void apply_hard_drop(World& world)
		{
			const Piece landing = shadow(world);
			const int rows = landing.y - world.current.y;

			world.score +=
				hard_drop_score * static_cast<std::uint32_t>(rows);
			world.current = landing;

			if (rows > 0)
			{
				// It moved, so it is not the turn that put it there any more.
				// A hard drop from rest is the ordinary way to finish a
				// T-spin, and that path keeps the flag because rows is zero.
				world.last_action_rotation = 0;
			}

			lock_piece(world);
		}

		void apply_gravity(World& world)
		{
			const bool soft = held(world, button_soft_drop);

			std::uint32_t rate = gravity_rate(world.level);

			if (soft)
			{
				rate *= soft_drop_multiplier;
			}

			world.gravity_sub_row += rate;

			// gravity_one_row is a power of two, so the compiler turns both of
			// these into a shift and a mask on the one path that runs every
			// tick of every game.
			const std::uint32_t rows = world.gravity_sub_row / gravity_one_row;
			world.gravity_sub_row %= gravity_one_row;

			for (std::uint32_t step = 0; step < rows; ++step)
			{
				Piece below = world.current;
				below.y = static_cast<std::int8_t>(below.y + 1);

				if (blocked(world, below))
				{
					// Landed. Whatever is left in the accumulator would make
					// the piece jump a row the instant it is slid off the
					// ledge, so it goes.
					world.gravity_sub_row = 0;
					break;
				}

				world.current = below;
				world.last_action_rotation = 0;

				// Falling a row is what buys the lock resets back.
				//
				// The guideline's rule is that they return when the piece
				// reaches a row lower than any it has occupied, which needs a
				// field this World does not have room for - the padding assert
				// prices new state in groups of four bytes, and one byte of
				// low-water mark would cost the memcmp its footing. The
				// difference is a player who kicks a piece upward and lets it
				// fall back to farm resets, which costs them more lock delay
				// than it buys.
				world.lock_timer = 0;
				world.lock_resets = 0;

				if (soft)
				{
					world.score += soft_drop_score;
				}
			}
		}

		void apply_lock_delay(World& world)
		{
			Piece below = world.current;
			below.y = static_cast<std::int8_t>(below.y + 1);

			if (!blocked(world, below))
			{
				// In the air. A piece slid off its ledge gets the whole delay
				// again when it next lands, and spends no reset doing it.
				world.lock_timer = 0;
				return;
			}

			++world.lock_timer;

			if (world.lock_timer >= lock_delay_ticks)
			{
				lock_piece(world);
			}
		}
	}

	void tick(World& world, std::uint8_t input)
	{
		// A finished match is finished. It does not advance its own tick
		// count, so the number is the length of the game rather than the
		// length of time the window was open.
		if (world.topped_out != 0)
		{
			return;
		}

		world.previous_input = world.input;
		world.input = input;
		++world.tick;

		if (world.current.kind == Kind::none)
		{
			spawn(world);

			if (world.topped_out != 0)
			{
				return;
			}
		}

		apply_hold(world);

		if (world.topped_out != 0)
		{
			return;
		}

		apply_rotation(world);
		apply_shift(world);

		if (pressed(world, button_hard_drop))
		{
			// Locks now, so there is no piece left for gravity to pull on and
			// no clock left to run down.
			apply_hard_drop(world);
			return;
		}

		apply_gravity(world);
		apply_lock_delay(world);
	}
}

#pragma once

#include "samples/linesweeper/rules/world.h"

#include <array>
#include <cstdint>

// The numbers the rules are made of: the seven shapes, the wall kicks, the
// gravity curve, the timings and the score table.
//
// It is a header of constants, which CONVENTIONS warns is usually tuning data
// in the wrong place - *which* and *how much* belong in JSON. It is not, and
// the reason is the wall this layer is built on: rules/ links no engine, so it
// has no JSON parser, no asset system and no file access, and a rule set that
// could be edited from disk would be a rule set a replay could not trust. A
// recorded game is only replayable against the tables it was played on, so
// these are compiled in and change by commit.
//
// Everything here is constexpr and every table is checked, at compile time,
// against the shape it is meant to have.
namespace linesweeper
{
	// --- The seven shapes ---
	//
	// GENERATED FROM SEVEN SPAWN STATES AND ONE FORMULA, not transcribed as
	// twenty-eight pictures. A clockwise turn inside a box of side n takes the
	// cell at (x, y) to (n - 1 - y, x), which is the whole of rotation; the
	// other three states of every piece fall out of applying it. Twenty-eight
	// hand-typed matrices is twenty-eight chances to put one square in the
	// wrong place, and no reader checks all of them. One formula and seven
	// spawn rows is checkable, and the static_asserts below check it.
	//
	// It also makes the O piece's standing invariant - that it does not budge
	// when you rotate it - structural instead of asserted. O's box is two
	// wide, and on a box of side two the formula maps the four cells onto
	// themselves. A three-wide box for O, which is the obvious thing to write,
	// slides it a column every turn.
	using PieceShape =
		std::array<std::array<Coord, piece_cell_count>, rotation_count>;
	using ShapeTable = std::array<PieceShape, kind_count + 1>;

	// The side of the bounding box each piece turns inside. Four for I because
	// it is four long, two for O, three for the rest - which is what the
	// published tables assume and is why the kicks below transcribe cleanly.
	constexpr int box_size(Kind kind)
	{
		switch (kind)
		{
		case Kind::i:
			return 4;
		case Kind::o:
			return 2;
		default:
			return 3;
		}
	}

	// Spawn state, read as a picture with the origin top-left and y down.
	constexpr std::array<Coord, piece_cell_count> spawn_shape(Kind kind)
	{
		switch (kind)
		{
		// ....
		// XXXX
		case Kind::i:
			return {{ {0, 1}, {1, 1}, {2, 1}, {3, 1} }};
		// X..
		// XXX
		case Kind::j:
			return {{ {0, 0}, {0, 1}, {1, 1}, {2, 1} }};
		// ..X
		// XXX
		case Kind::l:
			return {{ {2, 0}, {0, 1}, {1, 1}, {2, 1} }};
		// XX
		// XX
		case Kind::o:
			return {{ {0, 0}, {1, 0}, {0, 1}, {1, 1} }};
		// .XX
		// XX.
		case Kind::s:
			return {{ {1, 0}, {2, 0}, {0, 1}, {1, 1} }};
		// .X.
		// XXX
		case Kind::t:
			return {{ {1, 0}, {0, 1}, {1, 1}, {2, 1} }};
		// XX.
		// .XX
		case Kind::z:
			return {{ {0, 0}, {1, 0}, {1, 1}, {2, 1} }};
		default:
			return {{ {0, 0}, {0, 0}, {0, 0}, {0, 0} }};
		}
	}

	constexpr ShapeTable build_shapes()
	{
		ShapeTable table = {};

		for (int index = 1; index <= kind_count; ++index)
		{
			const Kind kind = static_cast<Kind>(index);
			const int size = box_size(kind);

			table[index][0] = spawn_shape(kind);

			for (int rotation = 1; rotation < rotation_count; ++rotation)
			{
				for (int cell = 0; cell < piece_cell_count; ++cell)
				{
					const Coord before = table[index][rotation - 1][cell];
					table[index][rotation][cell] = Coord{
						static_cast<std::int8_t>(size - 1 - before.y),
						before.x };
				}
			}
		}

		return table;
	}

	inline constexpr ShapeTable shapes = build_shapes();

	// The four cells of a piece in its own box. Kind::none is index zero and
	// four cells of (0, 0), so a caller that forgot to check gets a piece that
	// is nowhere rather than a read off the end.
	constexpr const std::array<Coord, piece_cell_count>& shape(Kind kind,
		int rotation)
	{
		return shapes[static_cast<int>(kind)][rotation];
	}

	// --- Checking the formula, once, at compile time ---
	//
	// Four pictures out of the twenty-eight, which is all it takes: if the
	// rotation is right for the three box sizes and O is invariant, it is
	// right everywhere, because the same three lines produced all of them.
	constexpr bool has_cell(const std::array<Coord, piece_cell_count>& cells,
		int x, int y)
	{
		for (int index = 0; index < piece_cell_count; ++index)
		{
			if (cells[index].x == x && cells[index].y == y)
			{
				return true;
			}
		}
		return false;
	}

	constexpr bool same_cells(const std::array<Coord, piece_cell_count>& left,
		const std::array<Coord, piece_cell_count>& right)
	{
		for (int index = 0; index < piece_cell_count; ++index)
		{
			if (!has_cell(left, right[index].x, right[index].y))
			{
				return false;
			}
		}
		return true;
	}

	// T, one turn clockwise:  .X.
	//                         .XX
	//                         .X.
	static_assert(has_cell(shape(Kind::t, 1), 1, 0));
	static_assert(has_cell(shape(Kind::t, 1), 1, 1));
	static_assert(has_cell(shape(Kind::t, 1), 2, 1));
	static_assert(has_cell(shape(Kind::t, 1), 1, 2));

	// I, upside down, is the third row of its four-wide box and not the
	// second - the half-cell shift that makes the I kicks their own table.
	static_assert(has_cell(shape(Kind::i, 2), 0, 2));
	static_assert(has_cell(shape(Kind::i, 2), 3, 2));

	// J, one turn anticlockwise (three clockwise): .X.
	//                                              .X.
	//                                              XX.
	static_assert(has_cell(shape(Kind::j, 3), 1, 0));
	static_assert(has_cell(shape(Kind::j, 3), 1, 1));
	static_assert(has_cell(shape(Kind::j, 3), 0, 2));
	static_assert(has_cell(shape(Kind::j, 3), 1, 2));

	// O stands still. This is the one the two-wide box buys.
	static_assert(same_cells(shape(Kind::o, 0), shape(Kind::o, 1)));
	static_assert(same_cells(shape(Kind::o, 0), shape(Kind::o, 2)));
	static_assert(same_cells(shape(Kind::o, 0), shape(Kind::o, 3)));

	// --- Where a piece starts ---
	//
	// Column three puts the three-wide boxes over columns 3-5 and I's
	// four-wide box over 3-6, which is where the guideline spawns them, and
	// row zero puts every piece wholly inside the two buffer rows. tick.cpp
	// then drops it one row if it can, so the player sees the new piece on the
	// frame it appears rather than a tick later.
	inline constexpr std::int8_t spawn_x = 3;
	inline constexpr std::int8_t spawn_y = 0;

	// --- The wall kicks ---
	//
	// The Super Rotation System's table, and the largest block of incidental
	// complexity in these rules. It is here in the form the sources print it -
	// y POSITIVE UPWARD - so that a reader can hold the file next to the
	// published table and compare rows without doing arithmetic in their head.
	// to_screen() below flips the sign of y exactly once, on the way into the
	// tables the code actually reads, because this well's row 0 is its top.
	//
	// A rotation tries the five offsets in order and takes the first that
	// fits. Failing all five is a rotation that does not happen.
	inline constexpr int kick_test_count = 5;
	inline constexpr int kick_transition_count = 8;
	using KickTable =
		std::array<std::array<Coord, kick_test_count>, kick_transition_count>;

	// Row order is `from * 2 + (clockwise ? 0 : 1)`, which is the eight
	// transitions the sources name 0>>R, 0>>L, R>>2, R>>0, 2>>L, 2>>R, L>>0
	// and L>>2 - in that order.
	constexpr int kick_row(int from, bool clockwise)
	{
		return from * 2 + (clockwise ? 0 : 1);
	}

	inline constexpr KickTable kicks_jlstz_published = {{
		// 0>>R
		{{ {0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2} }},
		// 0>>L
		{{ {0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2} }},
		// R>>2
		{{ {0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2} }},
		// R>>0
		{{ {0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2} }},
		// 2>>L
		{{ {0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2} }},
		// 2>>R
		{{ {0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2} }},
		// L>>0
		{{ {0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2} }},
		// L>>2
		{{ {0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2} }},
	}};

	inline constexpr KickTable kicks_i_published = {{
		// 0>>R
		{{ {0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2} }},
		// 0>>L
		{{ {0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1} }},
		// R>>2
		{{ {0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1} }},
		// R>>0
		{{ {0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2} }},
		// 2>>L
		{{ {0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2} }},
		// 2>>R
		{{ {0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1} }},
		// L>>0
		{{ {0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1} }},
		// L>>2
		{{ {0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2} }},
	}};

	// The one place the published convention meets this one.
	constexpr KickTable to_screen(const KickTable& published)
	{
		KickTable table = published;

		for (int row = 0; row < kick_transition_count; ++row)
		{
			for (int test = 0; test < kick_test_count; ++test)
			{
				table[row][test].y =
					static_cast<std::int8_t>(-table[row][test].y);
			}
		}

		return table;
	}

	inline constexpr KickTable kicks_jlstz = to_screen(kicks_jlstz_published);
	inline constexpr KickTable kicks_i = to_screen(kicks_i_published);

	// O has no kick table because O has no rotation to kick out of: its four
	// cells are the same set in all four states, so test one always fits and
	// the other four are never reached. It shares the JLSTZ table and never
	// leaves the first row of it.
	constexpr const KickTable& kicks(Kind kind)
	{
		return kind == Kind::i ? kicks_i : kicks_jlstz;
	}

	// The first test is a no-op in every transition of both tables - a
	// rotation that needs no kick is a rotation in place. If this ever fires,
	// a transcription slipped a row.
	constexpr bool first_test_is_identity(const KickTable& table)
	{
		for (int row = 0; row < kick_transition_count; ++row)
		{
			if (table[row][0].x != 0 || table[row][0].y != 0)
			{
				return false;
			}
		}
		return true;
	}

	static_assert(first_test_is_identity(kicks_jlstz));
	static_assert(first_test_is_identity(kicks_i));

	// A transition and its reverse are not inverses in SRS - that asymmetry is
	// the system's most surprising property and the source of half its
	// technique - but the sign of the first real test does flip, and getting
	// that wrong is the transcription error that produces a rotation system
	// which drifts left. Two spot checks, one table each.
	static_assert(kicks_jlstz[kick_row(0, true)][1].x == -1);
	static_assert(kicks_jlstz[kick_row(1, false)][1].x == 1);
	static_assert(kicks_i[kick_row(0, true)][1].x == -2);
	static_assert(kicks_i[kick_row(1, false)][1].x == 2);

	// The flip happened, and only to y. If the negation is ever deleted these
	// two fire and the whole kick set is upside down.
	static_assert(kicks_jlstz[kick_row(0, true)][3].y == 2);
	static_assert(kicks_jlstz_published[kick_row(0, true)][3].y == -2);

	// --- Gravity ---
	//
	// A row is divided into sub-rows so that the drop rate can be finer than
	// one row a tick without a float, which World forbids outright: a float in
	// the simulation makes every replay depend on /fp:precise and on this
	// compiler (world.h, the fourth assert). The accumulator adds a rate each
	// tick and spends whole rows out of it.
	//
	// A power of two so that "how many rows" and "what is left over" are a
	// shift and a mask rather than two divides, on the one path that runs
	// every tick of every game.
	inline constexpr std::uint32_t gravity_one_row = 65536;

	// Sub-rows per tick, one entry per level, at the sixty ticks a second
	// main.cpp pins. Derived from the guideline curve - a level's seconds per
	// row is (0.8 - 0.007 * level) raised to the level - and rounded to the
	// nearest sub-row; the frames per row each entry works out to is beside
	// it, because that is the number a player feels and the one worth
	// sanity-checking.
	//
	// The arithmetic that produced these ran once, in a spreadsheet, and its
	// output is checked in. Recomputing it here would need the float this
	// simulation is built not to have.
	inline constexpr int gravity_level_count = 15;
	inline constexpr std::array<std::uint32_t, gravity_level_count>
		gravity_table = {
			1092,    // level 1  - 60.0 ticks per row
			1377,    // level 2  - 47.6
			1768,    // level 3  - 37.1
			2310,    // level 4  - 28.4
			3075,    // level 5  - 21.3
			4169,    // level 6  - 15.7
			5759,    // level 7  - 11.4
			8107,    // level 8  -  8.1
			11635,   // level 9  -  5.6
			17027,   // level 10 -  3.8
			25416,   // level 11 -  2.6
			38709,   // level 12 -  1.7
			60171,   // level 13 -  1.1
			95487,   // level 14 -  0.7
			154745,  // level 15 -  0.4, which is two and a third rows a tick
		};

	// Levels are numbered from zero inside the World and displayed from one,
	// because `World{}` has to be a playable match: a value that needs a
	// constructor call before it means anything is not the value this sample
	// is about. The curve stops climbing at the last entry.
	inline constexpr std::uint8_t max_level = gravity_level_count - 1;
	inline constexpr std::uint32_t lines_per_level = 10;

	constexpr std::uint32_t gravity_rate(std::uint8_t level)
	{
		return gravity_table[level < max_level ? level : max_level];
	}

	// Holding soft drop multiplies gravity rather than replacing it, so it is
	// still faster than the curve at the top of it.
	inline constexpr std::uint32_t soft_drop_multiplier = 20;

	// --- Timings, all of them in ticks ---
	//
	// Ten ticks before a held direction starts repeating and two between
	// repeats: a sixth of a second to charge, thirty cells a second once it
	// has. Both are the numbers a falling-block player expects to be able to
	// change, and both are compiled in for the replay reason at the top of
	// this file.
	inline constexpr std::uint8_t shift_delay_ticks = 10;
	inline constexpr std::uint8_t shift_repeat_ticks = 2;

	// Half a second resting on the stack before the piece locks, and fifteen
	// chances to restart that clock by moving or turning.
	//
	// THE LOCK DELAY IS NOT OPTIONAL HERE, and README says why: a T-spin is
	// performed by rotating into the notch after the piece has landed, so a
	// game that locks on contact cannot express one. The cap is what stops a
	// player sliding a piece along the floor forever.
	inline constexpr std::uint8_t lock_delay_ticks = 30;
	inline constexpr std::uint8_t max_lock_resets = 15;

	// --- Scoring ---

	// What the last lock was, which is the only thing about it scoring needs
	// to know beyond how many rows went.
	enum class Spin : std::uint8_t
	{
		none = 0,
		// Three corners of the T's box filled, but only one of the two in
		// front of it. Worth less, because it is usually an accident.
		mini,
		full,
	};

	// Indexed by rows cleared. Multiplied by the level, which is why level is
	// one-based when it is scored with and zero-based when it is stored.
	inline constexpr std::array<std::uint32_t, 5> line_score =
		{ 0, 100, 300, 500, 800 };
	inline constexpr std::array<std::uint32_t, 5> mini_spin_score =
		{ 100, 200, 400, 0, 0 };
	inline constexpr std::array<std::uint32_t, 5> spin_score =
		{ 400, 800, 1200, 1600, 0 };

	// A clear that is worth chaining: a four-row clear, or any T-spin that
	// took a row with it. Two in a row is worth half as much again.
	//
	// Four rows, and it is four for its own reason rather than because a piece
	// has four cells - the two numbers happen to agree and one constant
	// answering both questions would be wrong the day either moved.
	inline constexpr int difficult_clear_rows = 4;
	inline constexpr std::uint32_t back_to_back_numerator = 3;
	inline constexpr std::uint32_t back_to_back_denominator = 2;

	inline constexpr std::uint32_t combo_score = 50;
	inline constexpr std::uint32_t soft_drop_score = 1;
	inline constexpr std::uint32_t hard_drop_score = 2;

	// Not scored: the perfect clear. It exists to reward a technique against
	// an opponent, and this game has none (README, What it is).
}

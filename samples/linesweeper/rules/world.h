#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

// The whole of a LineSweeper match, as one value.
//
// THIS HEADER INCLUDES NOTHING FROM engine/, AND THAT IS THE POINT. The rules
// layer is a static library that links artattack_settings and nothing else, so
// an engine symbol in here is a link error rather than a review comment. What
// it buys is that the entire game is playable inside the test suite, with no
// window, no device and no renderer - which is also what makes the replay test
// below possible at all.
//
// It is a value, not an object graph. Restarting is `world = World{}`. Saving a
// position is `World snapshot = world;`. Replaying is running the same inputs
// into a copy and comparing the bytes. None of those need a single line of code
// written for them, and that is the T11 demonstration this sample exists to
// make (PHILOSOPHY, T11).
namespace linesweeper
{
	// The well. Ten columns, twenty visible rows, and two rows above them that
	// a piece spawns into and the player never sees.
	//
	// The buffer rows are not decoration: a piece spawns flat across the top,
	// and without somewhere above the well to spawn into, the spawn test and
	// the top-out test are the same test, so a game that is merely full and a
	// game that is over cannot be told apart.
	inline constexpr int well_columns = 10;
	inline constexpr int well_visible_rows = 20;
	inline constexpr int well_buffer_rows = 2;
	inline constexpr int well_rows = well_visible_rows + well_buffer_rows;

	// Two seven-bags resident at once, so the preview can always show five
	// pieces without the bag refill being visible in the queue's ordering.
	//
	// Sixteen and not fourteen so the head index wraps with a mask rather than
	// a modulo. Two spare entries is two bytes; a divide on every push is not
	// the sort of thing this engine's sample should be teaching.
	inline constexpr int queue_capacity = 16;
	inline constexpr int queue_mask = queue_capacity - 1;

	// A cell is a byte: 0 is empty, 1..7 name the piece that filled it, which
	// is what the presentation colours it by. There is no Cell class, and a
	// grid of ten thousand of them would not want one either (PHILOSOPHY, The
	// object model).
	inline constexpr std::uint8_t cell_empty = 0;

	// The seven pieces, in the order the bag shuffles them.
	enum class Kind : std::uint8_t
	{
		none = 0,
		i, j, l, o, s, t, z,
	};
	inline constexpr int kind_count = 7;

	// Where a piece is and which way up. Four bytes.
	//
	// x and y are SIGNED because both legitimately go negative: a piece kicks
	// left past column zero during a wall kick and is pushed back, and a spawn
	// row is above the well's origin. An unsigned coordinate here turns the
	// most common off-by-one in the whole rule set into a wrap that silently
	// reads the far side of the board.
	struct Piece
	{
		Kind kind = Kind::none;
		std::uint8_t rotation = 0;
		std::int8_t x = 0;
		std::int8_t y = 0;
	};

	// The match.
	//
	// MEMBER ORDER IS BY SIZE, AND IT IS LOAD-BEARING. The four-byte members
	// come first, then the single bytes, then the arrays - which is what makes
	// the whole thing pack with no padding at all. See the asserts below for
	// why zero padding is a contract here rather than a tidiness.
	struct World
	{
		// Counts. tick is the fixed-step ordinal, so every duration in the
		// rules is denominated in ticks and none in seconds - which is what
		// keeps the simulation independent of how long a frame took.
		std::uint32_t tick = 0;
		std::uint32_t rng = 0;
		std::uint32_t score = 0;
		std::uint32_t lines = 0;
		// Gravity accumulates in fractions of a row so the drop speed can be
		// finer than one row per tick without a float. See tables.h.
		std::uint32_t gravity_sub_row = 0;

		std::uint8_t level = 0;
		std::uint8_t combo = 0;
		std::uint8_t back_to_back = 0;
		std::uint8_t lock_timer = 0;
		std::uint8_t lock_resets = 0;
		std::uint8_t shift_timer = 0;
		std::uint8_t shift_direction = 0;
		std::uint8_t hold_kind = 0;
		std::uint8_t hold_available = 0;
		std::uint8_t topped_out = 0;
		// T-spin detection needs to know that the piece arrived by rotating
		// rather than by moving or dropping, and the kick that got it there -
		// the last two kick offsets are the ones that force a full T-spin
		// rather than a mini. Nothing else in the rules reads these, and they
		// are the only cross-verb state in the simulation.
		std::uint8_t last_action_rotation = 0;
		std::uint8_t last_kick_index = 0;
		std::uint8_t input = 0;
		std::uint8_t previous_input = 0;
		std::uint8_t queue_head = 0;
		std::uint8_t queue_count = 0;

		Piece current;

		std::array<std::uint8_t, queue_capacity> queue = {};
		std::array<std::uint8_t, well_columns * well_rows> cells = {};
	};

	// THE FOUR ASSERTS, AND THE FOURTH IS THE INTERESTING ONE.
	//
	// The first three say what "the match is a value" means to the compiler:
	// it can be copied by memcpy, it needs no destructor, and therefore
	// `world = other` and `World snapshot = world` are complete operations
	// with nothing left to remember.
	//
	// has_unique_object_representations_v is the standard's own name for "this
	// type has no padding bits", which is what makes std::memcmp over a World
	// a defined comparison rather than a hopeful read of indeterminate bytes -
	// and the replay test in tests/linesweeper/ is exactly that memcmp.
	//
	// It also enforces something it does not mention. The trait is FALSE for
	// float and double, because two different bit patterns can compare equal.
	// So this one line is also what keeps the simulation integer-only, and it
	// fires on the day somebody adds a float "just for the lock timer" - which
	// would have made the replay depend on /fp:precise and on this compiler.
	// One trait, two invariants.
	static_assert(std::is_trivially_copyable_v<World>,
		"A World must be copyable by memcpy: restart is an assignment, and the "
		"replay test compares two of them byte for byte.");
	static_assert(std::is_trivially_destructible_v<World>,
		"A World must own nothing. A member that needs a destructor is a "
		"member that has an owner somewhere else.");
	static_assert(std::has_unique_object_representations_v<World>,
		"A World must have no padding bits and no floating-point members. "
		"memcmp is only a defined comparison without padding, and this trait "
		"is false for float - which is what keeps the rules integer-only and "
		"the replay bit-exact on any compiler.");
	static_assert(sizeof(World) == 276,
		"The match is meant to stay small enough to copy without thinking "
		"about it. If this fired because you added a member, read the number "
		"and update it. If it fired because you reordered them, the padding "
		"assert above is about to fire too.");

	// Whether two matches are the same match, compared as bytes.
	//
	// This is what the replay test asserts, and it is a function rather than a
	// std::memcmp at each call site so that the reason it is legal lives in
	// one place next to the assert that makes it so. It is only defined
	// because World has no padding bits; add one and the trait above fires
	// before this can start reading indeterminate bytes.
	//
	// Not operator==, deliberately. Byte equality and game equality are the
	// same thing here only because of an invariant three asserts up, and a
	// spelling that reads like an ordinary comparison would hide that.
	bool identical(const World& left, const World& right);

	// Index arithmetic, in one place. Row 0 is the top of the buffer, so y
	// increases downward and matches the screen - which is the opposite of the
	// published rotation tables and is exactly where a hand transcription goes
	// wrong. tables.h negates them once, on the way in, and says so.
	constexpr int cell_index(int x, int y)
	{
		return y * well_columns + x;
	}
}

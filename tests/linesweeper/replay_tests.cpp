#include <doctest/doctest.h>

#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"
#include "tests/linesweeper/well_fixtures.h"

#include <cstdint>
#include <vector>

using linesweeper::World;

namespace
{
	// A whole match, as a function of the tick number and nothing else.
	//
	// No clock, no random source, no file: the point of the tests below is
	// that the same bytes produce the same match, so the bytes have to come
	// from somewhere that cannot drift. Coprime periods mean the seven verbs
	// overlap in every combination the simulation has to survive - two
	// directions at once, a rotation on the tick a piece locks, a hold in the
	// middle of a charge.
	std::uint8_t scripted_input(std::uint32_t step)
	{
		std::uint8_t input = linesweeper::button_none;

		if (step % 2 == 0)
		{
			input |= linesweeper::button_soft_drop;
		}
		if (step % 3 == 0)
		{
			input |= linesweeper::button_left;
		}
		if (step % 5 == 0)
		{
			input |= linesweeper::button_right;
		}
		if (step % 7 == 0)
		{
			input |= linesweeper::button_rotate_clockwise;
		}
		if (step % 11 == 0)
		{
			input |= linesweeper::button_rotate_anticlockwise;
		}
		if (step % 13 == 0)
		{
			input |= linesweeper::button_hold;
		}
		if (step % 17 == 0)
		{
			input |= linesweeper::button_hard_drop;
		}

		return input;
	}

	void play(World& world, std::uint32_t from, std::uint32_t to)
	{
		for (std::uint32_t step = from; step < to; ++step)
		{
			linesweeper::tick(world, scripted_input(step));
		}
	}

	// THE TEST THIS WHOLE LAYER EXISTS FOR.
	//
	// It runs with no window, no device, no renderer and no engine linked into
	// the binary at all, and it compares two matches with std::memcmp - which
	// is defined only because World has no padding bits, which is what the
	// fourth static_assert in world.h is for.
	TEST_CASE("a match is its inputs")
	{
		SUBCASE("the same script twice is the same match, byte for byte")
		{
			World left;
			World right;

			play(left, 0, 4000);
			play(right, 0, 4000);

			CHECK(linesweeper::identical(left, right));

			// And it was a real game rather than four thousand ticks of
			// nothing: a script that hard drops every seventeenth tick fills
			// the well and eventually tops it out.
			CHECK(left.topped_out == 1);
			CHECK(left.score > 0);
		}

		SUBCASE("a snapshot resumes exactly, including the piece it deals next")
		{
			World world;
			play(world, 0, 200);

			const World snapshot = world;

			play(world, 200, 900);
			World resumed = snapshot;
			play(resumed, 200, 900);

			CHECK(linesweeper::identical(world, resumed));

			// The stream position travelled with the copy, which is the
			// reason World::rng is a counter and not an opaque register.
			CHECK(resumed.rng == world.rng);
		}

		SUBCASE("the recording is a vector of bytes and nothing else")
		{
			std::vector<std::uint8_t> recording;

			World recorded;
			for (std::uint32_t step = 0; step < 900; ++step)
			{
				const std::uint8_t input = scripted_input(step);
				recording.push_back(input);
				linesweeper::tick(recorded, input);
			}

			World replayed;
			for (std::size_t index = 0; index < recording.size(); ++index)
			{
				linesweeper::tick(replayed, recording[index]);
			}

			CHECK(linesweeper::identical(recorded, replayed));
		}
	}

	TEST_CASE("the seed is the only thing that changes the deal")
	{
		World fresh;
		linesweeper::tick(fresh, linesweeper::button_none);

		bool any_different = false;

		for (std::uint32_t seed = 1; seed <= 4; ++seed)
		{
			World seeded;
			seeded.rng = seed;
			linesweeper::tick(seeded, linesweeper::button_none);

			if (seeded.current.kind != fresh.current.kind)
			{
				any_different = true;
			}
		}

		// Zero is a legal seed and a different game from one, which an
		// xorshift could not have managed - it is stuck at zero forever from a
		// zero state, and `World{}` would have dealt no pieces at all.
		CHECK(any_different);
	}
}

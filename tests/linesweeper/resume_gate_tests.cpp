#include <doctest/doctest.h>

#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"
#include "samples/linesweeper/states/resume_gate.h"
#include "tests/linesweeper/well_fixtures.h"

#include <cstdint>

using linesweeper::Kind;
using linesweeper::Piece;
using linesweeper::ResumeGate;
using linesweeper::World;

// The pause menu's keys, kept out of the match until they are let go.
//
// This is the one file in the rules-only target that names anything under
// states/, and it can because resume_gate.h includes nothing but <cstdint>.
// What it pins is the leak the review found and the policy that closes it
// (docs/review/gpt6/README.md, G6-07), asserted through tick() rather than
// against the gate's bits alone: the first case is the review's reproduction
// and the second is the same bytes through the gate.

namespace
{
	// A piece resting mid-well with the match's input history at rest, which
	// is what a paused match looks like when Space was up at the pause.
	World paused_world()
	{
		World world;
		world.current = Piece{ Kind::t, 0, 3, 5 };
		linesweeper::tick(world, linesweeper::button_none);
		linesweeper::tick(world, linesweeper::button_none);
		return world;
	}
}

TEST_CASE("without the gate, the key that chose RESUME hard-drops the piece")
{
	// The reproduction. Space was up when the match paused, so the World's
	// own history has it up; the player confirms RESUME with Space and is
	// still holding it on the first tick back. To the rules that is a press,
	// because it is one: the byte before said up and this byte says down.
	// The rules are right, and this case exists so the next one is read as a
	// policy rather than as a fix to them.
	World world = paused_world();
	const int score_before = world.score;

	linesweeper::tick(world, linesweeper::button_hard_drop);

	CHECK(world.current.kind == Kind::none);
	CHECK(world.score > score_before);
}

TEST_CASE("through the gate, the key that chose RESUME does nothing until let go")
{
	World world = paused_world();
	ResumeGate gate;

	// The match came back with Space down.
	gate.close_over(linesweeper::button_hard_drop);

	// Held through the next ticks: no drop, no score, and the piece is where
	// it was left, whatever else gravity does to it.
	for (int frame = 0; frame < 5; ++frame)
	{
		linesweeper::tick(world, gate.pass(linesweeper::button_hard_drop));
	}
	CHECK(world.current.kind == Kind::t);
	CHECK(world.score == 0);

	// Let go, then press again: that one is the player's.
	linesweeper::tick(world, gate.pass(linesweeper::button_none));
	CHECK(world.current.kind == Kind::t);

	linesweeper::tick(world, gate.pass(linesweeper::button_hard_drop));
	CHECK(world.current.kind == Kind::none);
	CHECK(world.score > 0);
}

TEST_CASE("the gate covers RESTART's fresh world too")
{
	// A restart is `world = World{}` after the same confirm, so the World's
	// history is empty and the held key would be a press on the first dealt
	// piece. on_resume runs before the restart is applied
	// (state_context.h), so the gate is already closed over it.
	ResumeGate gate;
	gate.close_over(linesweeper::button_hard_drop);

	World world;
	linesweeper::tick(world, gate.pass(linesweeper::button_hard_drop));
	REQUIRE(world.current.kind != Kind::none);
	const Kind dealt = world.current.kind;

	for (int frame = 0; frame < 5; ++frame)
	{
		linesweeper::tick(world, gate.pass(linesweeper::button_hard_drop));
	}
	CHECK(world.current.kind == dealt);
	CHECK(world.score == 0);
}

TEST_CASE("a key held since before the pause is out until let go as well")
{
	// Down was held at the pause and through the menu. The policy is the
	// simple one: nothing held at the resume is the match's, so the soft
	// drop stops until Down is released and pressed again.
	ResumeGate gate;
	gate.close_over(linesweeper::button_soft_drop);

	CHECK(gate.pass(linesweeper::button_soft_drop) == linesweeper::button_none);
	CHECK(gate.pass(linesweeper::button_soft_drop) == linesweeper::button_none);
	CHECK(gate.pass(linesweeper::button_none) == linesweeper::button_none);
	CHECK(gate.pass(linesweeper::button_soft_drop) ==
		linesweeper::button_soft_drop);
}

TEST_CASE("each held key clears on its own, and a new key passes at once")
{
	// A and B on the pad are confirm and cancel and both rotate. Confirm
	// with A while Down is held: both are out. Let go of A, keep Down: A is
	// an ordinary key again and Down is still out. A press of Left in the
	// middle of all that is the player's and goes straight through.
	ResumeGate gate;
	gate.close_over(static_cast<std::uint8_t>(
		linesweeper::button_rotate_clockwise | linesweeper::button_soft_drop));

	CHECK(gate.pass(static_cast<std::uint8_t>(
		linesweeper::button_rotate_clockwise | linesweeper::button_soft_drop |
		linesweeper::button_left)) == linesweeper::button_left);

	CHECK(gate.pass(linesweeper::button_soft_drop) == linesweeper::button_none);
	CHECK(gate.suppressed() == linesweeper::button_soft_drop);

	CHECK(gate.pass(static_cast<std::uint8_t>(
		linesweeper::button_rotate_clockwise | linesweeper::button_soft_drop))
		== linesweeper::button_rotate_clockwise);
}

TEST_CASE("a gate that was closed over nothing passes everything")
{
	// Every frame of an ordinary match goes through it, so the no-op has
	// to be one.
	ResumeGate gate;
	CHECK(gate.pass(linesweeper::button_hard_drop) ==
		linesweeper::button_hard_drop);

	gate.close_over(linesweeper::button_none);
	CHECK(gate.pass(linesweeper::button_left) == linesweeper::button_left);
}

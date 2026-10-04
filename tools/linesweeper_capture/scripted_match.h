#pragma once

#include "samples/linesweeper/rules/world.h"
#include "tools/linesweeper_capture/capture_script.h"

#include <cstdint>
#include <functional>

namespace capture
{
	// A LineSweeper match, steered by script commands one tick at a time.
	//
	// EVERY COMMAND BECOMES THE BYTES tick() TAKES, and nothing else reaches
	// the match. A press is one tick with the button's bit set and the next
	// tick with it clear - which is what a key held for one frame produces in
	// play_state.cpp - so every edge is one the rules derive for themselves
	// (samples/linesweeper/README.md, Input is read as held, never as
	// pressed). Steering reads the World to decide which button comes next,
	// the way a player looks at the screen; it never writes one.
	//
	// IT NEEDS NO ENGINE, only the rules library it drives, so a whole script
	// can be stepped without a window or a device.
	class ScriptedMatch
	{
	public:
		// `after_tick` runs once after every tick, with the World already
		// stepped and last_tick() already what that tick returned. It is where
		// the presentation's update goes, and it is called on every tick
		// rather than once per command so the particle field steps exactly as
		// it does in the sample: one update per tick.
		explicit ScriptedMatch(std::function<void()> after_tick);

		// Borrowed by the presentation for as long as this object lives: the
		// addresses never change, as PlayState's members' do not.
		const linesweeper::World& world() const;
		const linesweeper::TickResult& last_tick() const;

		// Runs place, steer, drop, hold or wait; a capture is the caller's.
		//
		// Throws std::runtime_error naming the script line when the match is
		// not in the state the command needs - a different piece falling, a
		// move the well blocks, a match already over - because an image of the
		// game the script did not describe is the failure this tool exists to
		// prevent.
		void run(const Command& command);

	private:
		void step(std::uint8_t input);

		// Pressed for one tick, released for the next.
		void tap(std::uint8_t button);

		// Deals the next piece if none is falling and checks it is `expected`.
		// Kind::none checks nothing.
		void take_piece(const Command& command, linesweeper::Kind expected);

		void steer(const Command& command);
		void drop(const Command& command);
		void hold(const Command& command);

		std::function<void()> after_tick_;
		linesweeper::World world_;
		linesweeper::TickResult last_tick_;

		// How many ticks have locked a piece. Steering compares it before and
		// after, because a piece that locks mid-steer can be followed by
		// another of the same kind, and comparing kinds would miss that.
		int locks_ = 0;
	};
}

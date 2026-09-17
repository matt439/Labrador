#pragma once

#include <cstdint>

// What the pause menu consumed, kept out of the match until it is let go.
//
// SPACE CONFIRMS RESUME AND SPACE IS HARD DROP. The menu reads a press; the
// match reads what is held; and the key is still down on the first tick after
// the menu closes. The rules derive their own edges - down this tick, up last
// tick, from the byte handed to tick() against the byte before it - so a key
// that was up when the match was suspended and down when it came back is a
// press to the simulation, and the piece the player was looking at when they
// paused went through the floor the instant they chose RESUME. A on the pad
// confirms and rotates; B cancels and rotates the other way; the same leak,
// three keys wide (docs/review/gpt6/README.md, G6-07).
//
// The fix is a mask on the play state's side, not an edge on the menu's,
// because the byte tick() gets has to stay a record of what was held: the
// replay tests are a match reproduced from its bytes, and a byte that meant
// "held, unless a menu was open recently" is not a byte a recording can
// carry. So the state remembers what was down when the match came back and
// leaves each of those bits out of the byte until that key has been released
// - at which point it is an ordinary key again and the next press of it is
// the player's. Nothing here is a state of the simulation, and nothing here
// is a rule: it is the one policy states/ owns about how a device's history
// meets a World's, which is why it lives beside the two tables that turn the
// devices into the byte.
//
// EVERYTHING HELD AT THE MOMENT OF RESUME, NOT ONLY THE MENU'S KEYS. A player
// holding Down when they paused, and still holding it, gets no soft drop
// until they let go and press again. That is the simpler rule (T3) and the
// one a player can predict: the match resumes from nothing, whatever the
// hands were doing. The same mask covers RESTART, which the menu also
// confirms with the same key over a fresh World whose own input history is
// empty - the leak there was a hard drop on the first dealt piece.
//
// Header-only and engine-free on purpose: the tests for it link the rules
// and nothing else, so what it does to a byte can be asserted against tick()
// with no window, which is where the review found the leak in the first
// place.
namespace linesweeper
{
	class ResumeGate
	{
	public:
		// The match is back and these bits are down. Each of them is kept out
		// of the byte until the player lets go of it.
		void close_over(std::uint8_t held)
		{
			this->suppressed_ = held;
		}

		// What tick() gets this frame, given what is held: everything the
		// player is pushing except what they have been pushing since before
		// the match came back. A bit clears the first frame it is not held.
		std::uint8_t pass(std::uint8_t held)
		{
			this->suppressed_ &= held;
			return static_cast<std::uint8_t>(held & ~this->suppressed_);
		}

		std::uint8_t suppressed() const
		{
			return this->suppressed_;
		}

	private:
		std::uint8_t suppressed_ = 0;
	};
}

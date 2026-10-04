#pragma once

#include "samples/linesweeper/rules/world.h"

#include <filesystem>
#include <string>
#include <vector>

// The capture script: a LineSweeper match written as the placements a player
// would make, with the frames to read back marked where they fall.
//
// TEXT, ONE COMMAND A LINE, because it is reviewed by reading it. A line says
// which piece goes where, so a change to the match is a change to a line and a
// diff of the script is a diff of the game. A recording of raw input bytes
// would replay just as exactly and could not be read or edited by anybody:
// moving one piece one column would mean re-deriving every byte after it.
//
// tools/linesweeper_capture/README.md is the reference for the format.
namespace capture
{
	enum class CommandKind
	{
		place,
		steer,
		drop,
		hold,
		wait,
		capture,
	};

	struct Command
	{
		CommandKind kind = CommandKind::wait;

		// The piece place, steer and hold expect to be falling. Checked, so a
		// script that has drifted from the deal fails on the line that drifted
		// rather than quietly drawing a different game.
		linesweeper::Kind piece = linesweeper::Kind::none;

		// place and steer: clockwise quarter turns from the spawn orientation,
		// 0 to 3, and the leftmost column the piece's cells occupy afterwards,
		// 0 to 9.
		int turns = 0;
		int column = 0;

		// wait: how many ticks pass with nothing held. At least one.
		int ticks = 0;

		// capture: a bare file name ending in .png, written into the output
		// directory, and whether the pause menu is drawn over the frame.
		std::string file;
		bool paused = false;

		// The script line the command came from, for every error that names it.
		int line = 0;
	};

	// Reads and validates the whole script before anything runs, so a typo on
	// the last line costs nothing rather than a device and a half-written set.
	//
	// Throws std::runtime_error naming the file and the line for anything it
	// does not understand, and for a script with no capture in it.
	std::vector<Command> read_script(const std::filesystem::path& path);

	// The letter the script spells a piece with: i, j, l, o, s, t or z.
	char kind_letter(linesweeper::Kind kind);
}

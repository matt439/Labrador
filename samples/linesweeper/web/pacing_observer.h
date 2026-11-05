#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace labrador
{
	class Application;
}

namespace linesweeper
{
	enum class PacingMode
	{
		disabled,
		play,
		top_out,
		top_out_frozen,
	};

	// Owned beside the static browser Application, never by the measured state.
	// update() writes it; the rAF observer reads it between application ticks.
	struct PacingSnapshot
	{
		std::uint64_t top_out_frame = 0;
		int live_particles = 0;
		std::uint32_t dropped_particles = 0;
	};

	// The mode the page's integer names, 0 to 3 in PacingMode's order. Any
	// other value throws std::invalid_argument rather than becoming an
	// unnamed enumerator: a page asking for a fixture this build does not
	// know fails instead of measuring ordinary play.
	inline PacingMode pacing_mode_from(int value)
	{
		switch (value)
		{
		case 0:
			return PacingMode::disabled;
		case 1:
			return PacingMode::play;
		case 2:
			return PacingMode::top_out;
		case 3:
			return PacingMode::top_out_frozen;
		default:
			throw std::invalid_argument("Unknown LineSweeper pacing mode " +
				std::to_string(value) + "; the page may ask for 0 to 3.");
		}
	}

	// The development page's pre-js enables this. The packaged module has no
	// such script, so a page URL alone cannot select a fixture in that module.
	// Throws as pacing_mode_from does.
	PacingMode pacing_mode();

	// A separate rAF callback, not a fixed-step update or a draw worker. The
	// page deduplicates completed frame ids and retains observed gaps.
	void observe_pacing(labrador::Application& app, const PacingSnapshot& snapshot);
}

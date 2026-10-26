#pragma once

#include <cstdint>

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

	// The development page's pre-js enables this. The packaged module has no
	// such script, so a page URL alone cannot select a fixture in that module.
	PacingMode pacing_mode();

	// A separate rAF callback, not a fixed-step update or a draw worker. The
	// page deduplicates completed frame ids and retains observed gaps.
	void observe_pacing(labrador::Application& app, const PacingSnapshot& snapshot);
}

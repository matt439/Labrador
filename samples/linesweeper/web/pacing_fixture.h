#pragma once

#include "samples/linesweeper/rules/world.h"

namespace linesweeper
{
	class ParticleField;

	// web_full_well_top_out_first_burst_v1: all 200 visible cells, using the
	// native frame benchmark's board and HUD values. The browser's evolving
	// burst is a separate workload from the benchmark's frozen zero-time frame.
	inline constexpr int pacing_particle_count = 9600;

	// A pre-top-out world. Construct the particle field against this value
	// before starting the transition, so it observes the full well as a burst
	// rather than as cells appearing during an ordinary tick.
	World pacing_world();

	// Marks top-out on the next tick. Update the field with zero elapsed time
	// to materialize the exact first burst before timing its subsequent decay.
	void start_pacing_top_out(World& world);

	// Throws std::runtime_error with the actual counts unless the field has
	// exactly pacing_particle_count live particles and no refused emissions.
	void require_pacing_particles(const ParticleField& particles);
}

#include "samples/linesweeper/web/pacing_fixture.h"

#include "samples/linesweeper/presentation/particles.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace linesweeper
{
	World pacing_world()
	{
		World world;
		world.tick = 1000;
		world.score = 999999;
		world.lines = 199;
		world.level = 14;
		world.hold_kind = static_cast<std::uint8_t>(Kind::o);
		world.hold_available = 1;

		for (int index = 0; index < 5; ++index)
		{
			world.queue[static_cast<std::size_t>(index)] =
				static_cast<std::uint8_t>(index % kind_count + 1);
		}
		world.queue_count = 5;

		for (int y = well_buffer_rows; y < well_rows; ++y)
		{
			for (int x = 0; x < well_columns; ++x)
			{
				world.cells[static_cast<std::size_t>(cell_index(x, y))] =
					static_cast<std::uint8_t>((x + y) % kind_count + 1);
			}
		}
		return world;
	}

	void start_pacing_top_out(World& world)
	{
		world.topped_out = 1;
		++world.tick;
	}

	void require_pacing_particles(const ParticleField& particles)
	{
		if (particles.live() != pacing_particle_count || particles.dropped() != 0)
		{
			throw std::runtime_error(
				"The browser pacing top-out must have exactly 9,600 live particles "
				"and zero dropped emissions; got " + std::to_string(particles.live()) +
				" live and " + std::to_string(particles.dropped()) + " dropped.");
		}
	}
}

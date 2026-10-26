#include "samples/linesweeper/web/pacing_fixture.h"

#include "engine/render/renderer.h"
#include "samples/linesweeper/presentation/particles.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

using labrador::TextureHandle;
using linesweeper::ParticleField;
using linesweeper::TickResult;
using linesweeper::World;
using linesweeper::cell_empty;
using linesweeper::cell_index;
using linesweeper::pacing_particle_count;
using linesweeper::pacing_world;
using linesweeper::require_pacing_particles;
using linesweeper::start_pacing_top_out;
using linesweeper::well_buffer_rows;
using linesweeper::well_columns;
using linesweeper::well_rows;

TEST_CASE("the browser pacing world has the benchmark's exact visible well and HUD")
{
	const World world = pacing_world();
	int visible_cells = 0;
	for (int y = 0; y < well_rows; ++y)
	{
		for (int x = 0; x < well_columns; ++x)
		{
			const std::uint8_t cell =
				world.cells[static_cast<std::size_t>(cell_index(x, y))];
			if (y < well_buffer_rows)
			{
				CHECK(cell == cell_empty);
			}
			else
			{
				CHECK(cell == static_cast<std::uint8_t>((x + y) % 7 + 1));
				++visible_cells;
			}
		}
	}
	CHECK(visible_cells == 200);
	CHECK(world.tick == 1000);
	CHECK(world.score == 999999);
	CHECK(world.lines == 199);
	CHECK(world.level == 14);
	CHECK(world.hold_kind == static_cast<std::uint8_t>(linesweeper::Kind::o));
	CHECK(world.hold_available == 1);
	CHECK(world.topped_out == 0);
	CHECK(world.current.kind == linesweeper::Kind::none);
	CHECK(world.queue_head == 0);
	CHECK(world.queue_count == 5);
	for (int index = 0; index < linesweeper::queue_capacity; ++index)
	{
		CHECK(world.queue[static_cast<std::size_t>(index)] ==
			static_cast<std::uint8_t>(index < 5 ? index + 1 : 0));
	}
}

TEST_CASE("the browser pacing transition creates an exact first burst without a device")
{
	World world = pacing_world();
	TickResult result;
	ParticleField particles(&world, &result, TextureHandle{});
	CHECK(particles.live() == 0);
	CHECK(particles.dropped() == 0);
	CHECK_THROWS_AS(require_pacing_particles(particles), std::runtime_error);

	start_pacing_top_out(world);
	CHECK(world.tick == 1001);
	CHECK(world.topped_out == 1);
	particles.update(0.0f);
	CHECK(particles.live() == pacing_particle_count);
	CHECK(particles.dropped() == 0);
	CHECK_NOTHROW(require_pacing_particles(particles));

	// A normal fixed step starts decay without changing the first burst's count.
	++world.tick;
	particles.update(1.0f / 60.0f);
	CHECK(particles.live() == pacing_particle_count);
	CHECK(particles.dropped() == 0);
	CHECK_NOTHROW(require_pacing_particles(particles));
}

TEST_CASE("the browser pacing validator refuses a top-out from an incomplete well")
{
	World world = pacing_world();
	world.cells[static_cast<std::size_t>(cell_index(0, well_buffer_rows))] = cell_empty;
	TickResult result;
	ParticleField particles(&world, &result, TextureHandle{});
	start_pacing_top_out(world);
	particles.update(0.0f);
	REQUIRE(particles.live() > 0);
	CHECK(particles.live() < pacing_particle_count);
	CHECK(particles.dropped() == 0);
	CHECK_THROWS_WITH_AS(require_pacing_particles(particles),
		"The browser pacing top-out must have exactly 9,600 live particles "
		"and zero dropped emissions; got 9552 live and 0 dropped.",
		std::runtime_error);
}

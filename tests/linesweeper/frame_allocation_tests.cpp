#include <doctest/doctest.h>

#include "tests/allocation_scope.h"

// LineSweeper's frames held to PHILOSOPHY's Performance rule that per-frame
// code performs no heap allocation: the whole sample scene the frame benchmark
// draws - board, HUD, particle field, top-out banner and hint labels - through
// a restart, the top-out burst of 9,600 sparks and its decay, and a HUD whose
// numbers change.
//
// The first top-out warms the renderer's buffers to the burst's peak; the
// second is counted from the restart on, so the frames that change state are
// measured and not only the steady ones.
//
// Debug builds against the DLL CRT only, where allocation_scope.h has its
// hook; everywhere else this file compiles to nothing.
#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/label.h"
#include "engine/render/null/recording.h"
#include "engine/render/renderer.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"
#include "samples/linesweeper/presentation/assets.h"
#include "samples/linesweeper/presentation/board_view.h"
#include "samples/linesweeper/presentation/hints.h"
#include "samples/linesweeper/presentation/palette.h"
#include "samples/linesweeper/presentation/particles.h"
#include "samples/linesweeper/presentation/top_out_banner.h"
#include "samples/linesweeper/rules/tick.h"
#include "samples/linesweeper/rules/world.h"
#include "samples/linesweeper/web/pacing_fixture.h"
#include "tests/linesweeper/null_render_harness.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace
{
	using namespace labrador;
	using namespace linesweeper;

	// The frame benchmark's scene (bench/linesweeper_frame_bench.cpp), over
	// the null renderer.
	class SampleScene
	{
	public:
		explicit SampleScene(const World& start) :
			world(start),
			scene_(nullptr, nullptr)
		{
			this->scene_.add(std::make_unique<BoardView>(&this->world,
				&this->harness_.resources(), &standard_palette));
			this->particles = this->scene_.add(std::make_unique<ParticleField>(
				&this->world, &this->tick_, this->harness_.white));
			this->scene_.add(std::make_unique<TopOutBanner>(&this->world,
				&this->harness_.resources()));
			this->scene_.add(std::make_unique<Label>(keyboard_hint_text,
				font_name, Vector2F(hint_x, keyboard_hint_y),
				&this->harness_.resources(), Colour::dark_gray));
			this->scene_.add(std::make_unique<Label>(pad_hint_text, font_name,
				Vector2F(hint_x, pad_hint_y), &this->harness_.resources(),
				Colour::dark_gray));
			this->scene_.end_tick();
			this->scene_.add_view(Viewport(0.0f, 0.0f, 1280.0f, 720.0f));
		}

		// One frame as the sample runs one: the scene's tick, then its draw.
		void frame(float dt)
		{
			this->scene_.update(dt);
			this->scene_.resolve();
			this->scene_.end_tick();
			Renderer& renderer = this->harness_.renderer();
			renderer.begin_frame();
			this->scene_.draw(renderer);
			renderer.submit();
		}

		size_t recorded() { return recorded_sprites(this->harness_.renderer()).size(); }

		World world;
		ParticleField* particles = nullptr;

	private:
		linesweeper_tests::NullHarness harness_;
		TickResult tick_;
		Scene scene_;
	};

	void require_no_allocation(SampleScene& top_out, const char* what,
		float dt)
	{
		const labrador_tests::AllocationSample sample =
			labrador_tests::count_allocations([&]() { top_out.frame(dt); });
		INFO(what, ": normal-block attempts ", sample.attempts,
			", requested bytes ", sample.bytes);
		CHECK(sample.attempts == 0);
	}

	constexpr float step = 1.0f / 60.0f;
	constexpr int decay_frames = 60;
}

TEST_CASE("LineSweeper's restart, top-out burst and decay allocate nothing once warmed")
{
	labrador_tests::require_allocation_scope_counts();

	SampleScene top_out(pacing_world());
	for (int frame = 0; frame < 4; ++frame)
	{
		top_out.frame(step);
	}

	// The warming top-out: the burst at its peak and the frames after it.
	start_pacing_top_out(top_out.world);
	top_out.frame(0.0f);
	require_pacing_particles(*top_out.particles);
	for (int frame = 0; frame < decay_frames; ++frame)
	{
		++top_out.world.tick;
		top_out.frame(step);
	}

	// A restart is `world = World{}`-shaped: the tick goes backwards and the
	// field and banner clear.
	top_out.world = pacing_world();
	require_no_allocation(top_out, "restart", step);
	CHECK(top_out.particles->live() == 0);

	start_pacing_top_out(top_out.world);
	require_no_allocation(top_out, "top-out burst", 0.0f);
	require_pacing_particles(*top_out.particles);
	CHECK(top_out.recorded() > static_cast<size_t>(pacing_particle_count));

	for (int frame = 0; frame < decay_frames; ++frame)
	{
		CAPTURE(frame);
		++top_out.world.tick;
		require_no_allocation(top_out, "decay", step);
	}
	CHECK(top_out.particles->live() > 0);
}

TEST_CASE("LineSweeper's HUD reformats changed numbers without allocating")
{
	labrador_tests::require_allocation_scope_counts();

	// An empty well, so a jump in the line count clears no row.
	SampleScene play{ World{} };

	// Every digit a 32-bit score can show, and the widest lines and level,
	// which is more than a string's own small buffer holds.
	const auto widest = [&]()
	{
		play.world.score = (std::numeric_limits<std::uint32_t>::max)();
		play.world.lines = (std::numeric_limits<std::uint32_t>::max)();
		play.world.level = static_cast<std::uint8_t>(
			(std::numeric_limits<std::uint8_t>::max)() - 1);
	};
	const auto zero = [&]()
	{
		play.world.score = 0;
		play.world.lines = 0;
		play.world.level = 0;
	};

	// Frames of the widest HUD warm the renderer to its most glyphs: growth
	// past a peak no frame has drawn is not what this asks about.
	widest();
	for (int frame = 0; frame < 4; ++frame)
	{
		play.frame(step);
	}

	for (int round = 0; round < 2; ++round)
	{
		CAPTURE(round);
		zero();
		require_no_allocation(play, "back to zero", step);
		widest();
		require_no_allocation(play, "widest numbers", step);
	}
}

#endif

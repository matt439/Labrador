#include "bench/bench.h"

#include "engine/render/null/recording.h"
#include "engine/collision/partitioner.h"
#include "engine/core/game_object.h"
#include "engine/core/thread_pool.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/render/viewport.h"
#include "engine/scene/scene.h"

#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

// The per-view render fan-out, run.
//
// THIS IS THE FIRST THING IN THIS REPOSITORY THAT TAKES THE PARALLEL BRANCH.
// PHILOSOPHY commits to one axis of parallelism "and that one alone", and
// scene.cpp returns before the fan-out when the view count is one or the pool
// is null or the partitioner is null. Both samples construct
// Scene(nullptr, nullptr); nothing in tests/ or bench/ ever handed a scene a
// pool. So the early-out was the only branch that had ever been taken, on any
// machine - which is worse than untested and different from it, because the
// rule the conventions are built around governed code that had not run.
// docs/survey/2026-08-26.md 2.1 is the finding and this file is half of its
// answer; tests/scene/fanout_tests.cpp is the other half, and pins what the
// fan-out must produce where this only measures what it costs.
//
// COMPILED ONLY IN THE null CONFIGURATION, which is why it is its own file
// rather than a case in scene_bench.cpp. Scene::draw needs a Renderer, and the
// only Renderer that exists without a window and an adapter is this one -
// bench/CMakeLists.txt lists this file or fanout_bench_absent.cpp beside it,
// never both, and that file is where the four configurations without a fan-out
// case say so out loud. scene_bench.cpp's hand-copied cull stays exactly as it
// is: it builds in all five, and its comment already says why a duplicated
// loop beats a case that measures a different thing depending on the preset.
//
// WHAT THE NUMBER IS AND IS NOT. Under this backend a draw is an entry pushed
// into a vector - engine/render/null/recording.h - so what the workers are
// dividing is the cull, the quad arithmetic and that push, and nothing a
// device would have done with the result. That is fine for complexity class,
// which is all bench.h asserts on, and it is the honest floor for the fan-out
// itself: the work per view here is the work the engine does per view, and a
// backend adding its own would only widen the gap the ratio reports. It is not
// a frame time and no split-screen frame rate can be predicted from it.
//
// THE RATIO main.cpp PRINTS CAN BE BELOW ONE, AND THAT IS THE FINDING RATHER
// THAN A FAULT. Cutting the view list up, submitting a task per range and
// waiting costs the same whether there are sixty objects or sixty thousand, so
// at the small end the fan-out pays a fixed price for work it barely divides
// and comes out behind one thread. The count where the ratio crosses one is
// the number to read out of this case: below it the early-out at scene.cpp is
// not a limitation but the faster path, and above it the fan-out earns its
// keep. The counts here bracket that crossing deliberately.
//
// AND ctest MEASURES IT IN A DEBUG BUILD, because x64-debug-null is the only
// preset that builds this backend and CMakePresets.json has no release one.
// That flatters the fan-out: an unoptimised per-view body is more work for the
// workers to divide, so the crossing sits at a lower object count than a
// release build of the same backend puts it. Configuring one by hand
// (-DLABRADOR_RENDER_BACKEND=null -DCMAKE_BUILD_TYPE=Release) is how that was
// checked, and anybody quoting a crossover from a ctest run is quoting the
// friendlier of the two numbers.

using labrador::Camera;
using labrador::Colour;
using labrador::DrawList;
using labrador::GameObject;
using labrador::Partitioner;
using labrador::RenderResources;
using labrador::Renderer;
using labrador::Scene;
using labrador::SpriteFlip;
using labrador::TextureData;
using labrador::TextureFormat;
using labrador::TextureHandle;
using labrador::ThreadPool;
using labrador::Viewport;
using mattmath::RectangleF;
using mattmath::RectangleI;
using mattmath::Vector2F;

namespace
{
	// Four panes, because that is what the fan-out was written for.
	// ApplicationOptions::view_capacity is 4 and says why: "Four is four-player
	// split-screen, which is the widest layout either client has."
	constexpr int view_count = 4;

	// A 1280x720 back buffer cut into four of them.
	constexpr float pane_width = 640.0f;
	constexpr float pane_height = 360.0f;

	// The same arena shape scene_bench.cpp builds, for the reason it gives: a
	// grid is what a laid-out level is, and a random scatter would measure the
	// random number generator's cache behaviour as much as the phase.
	constexpr float cell = 64.0f;
	constexpr float object_size = 48.0f;

	// A renderer with no graphics API, and a texture that needed no file.
	//
	// create_device takes a window handle this backend ignores, which is the
	// whole reason this can run where there is no display. The texture is built
	// by hand and handed to add_texture_asset the way
	// tests/render/renderer_seam_tests.cpp builds its own: a benchmark with a
	// content directory beside it is a benchmark that can fail for a reason
	// that is not about the code, and two texels is all a recording needs.
	class Harness
	{
	public:
		Harness()
		{
			this->renderer_.create_device(nullptr, 1280, 720, view_count);
			this->renderer_.set_resources(&this->resources_);

			TextureData texture;
			texture.width = 2;
			texture.height = 2;
			texture.format = TextureFormat::r8g8b8a8_unorm;
			texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
			texture.pixels.assign(texture.levels[0].size, 0xFFu);

			add_texture_asset(this->renderer_, this->resources_, "quad",
				texture);
			this->quad = this->resources_.resolve_texture("quad");
		}

		Renderer& renderer() { return this->renderer_; }

		TextureHandle quad;

	private:
		// The table before the renderer, so it dies after one -
		// render_resources.h states that ordering as a term of the seam.
		RenderResources resources_;
		Renderer renderer_;
	};

	class FanOutObject final : public GameObject
	{
	public:
		FanOutObject(const RectangleF& rectangle, TextureHandle texture) :
			rectangle_(rectangle), texture_(texture)
		{
		}

		void update(float /*dt*/) override {}

		// THE PURE READ THE WHOLE AXIS RESTS ON, asked for at last. Several
		// workers enter this on the same object at the same time - that is
		// what a view fan-out is, and it is why GameObject::draw is const -
		// so it reads two members and writes nothing. Until this file the
		// discipline had never been exercised by anything.
		void draw(DrawList& list) const override
		{
			list.draw_sprite(this->texture_, RectangleI(0, 0, 2, 2),
				this->rectangle_, Colour::white, 0.0f, Vector2F::ZERO,
				SpriteFlip::none, 0.0f);
		}

		RectangleF bounds() const override { return this->rectangle_; }

	private:
		RectangleF rectangle_;
		TextureHandle texture_;
	};

	// One arena, four cameras over quarters of it, and a pool or not.
	//
	// The two scenes a run compares differ in the constructor arguments and in
	// nothing else - same objects at the same places, same views, same
	// cameras - so the gap between the two rows is the fan-out and not the
	// scene.
	//
	// THE PANES OVERLAP AND THEY HAVE TO. Camera::frame fits a rectangle into a
	// viewport by the tighter of the two ratios, so a square quarter of a
	// square world shown in a 16:9 pane comes with surplus on the wide axis.
	// That surplus is the same fraction of the world at every count, because
	// everything here scales with the same side, so it shifts the curve up and
	// does not bend it - which is what an assertion on complexity class cares
	// about. It is also what a split-screen actually looks like.
	std::unique_ptr<Scene> build_scene(int count, TextureHandle texture,
		ThreadPool* pool, const Partitioner* partitioner)
	{
		std::unique_ptr<Scene> scene =
			std::make_unique<Scene>(pool, partitioner);

		int side = 1;
		while (side * side < count)
		{
			side++;
		}

		for (int i = 0; i < count; i++)
		{
			const RectangleF place(static_cast<float>(i % side) * cell,
				static_cast<float>(i / side) * cell,
				object_size, object_size);
			scene->add(std::make_unique<FanOutObject>(place, texture));
		}
		scene->end_tick();

		const float half = static_cast<float>(side) * cell / 2.0f;

		for (int i = 0; i < view_count; i++)
		{
			const Viewport viewport(
				static_cast<float>(i % 2) * pane_width,
				static_cast<float>(i / 2) * pane_height,
				pane_width, pane_height);

			const RectangleF quarter(static_cast<float>(i % 2) * half,
				static_cast<float>(i / 2) * half, half, half);

			scene->add_view(viewport, Camera::frame(quarter, viewport));
		}

		return scene;
	}

	// Which thread drew each view, taken once and outside the timing.
	//
	// REPORTED AND NOT ASSERTED ON, and tests/core/thread_pool_tests.cpp is
	// where that standard is set: "NOTHING HERE ASSERTS ON PARALLELISM,
	// deliberately. A pool with a maximum of four threads is allowed to run
	// four tasks on one thread, and a test that demanded otherwise would fail
	// on a machine that decided differently." So this is an observation of one
	// run on one machine, it may legitimately come back as one thread, and it
	// is printed rather than checked.
	//
	// It is safe to read afterwards for the seam's own reasons: the overlay
	// "runs on the worker that owns that view" (scene.h), one worker owns each
	// index, so the slots are disjoint - and Scene::draw waits for every task
	// before it returns, which is the happens-before this read needs.
	std::string observe_threads(Harness& harness, const Partitioner& partitioner,
		ThreadPool& pool)
	{
		std::vector<std::thread::id> drawn_by(
			static_cast<size_t>(view_count));

		const std::unique_ptr<Scene> scene =
			build_scene(4096, harness.quad, &pool, &partitioner);

		harness.renderer().begin_frame();
		scene->draw(harness.renderer(),
			[&drawn_by](int index, DrawList&)
			{
				drawn_by[static_cast<size_t>(index)] =
					std::this_thread::get_id();
			});

		const std::set<std::thread::id> distinct(drawn_by.begin(),
			drawn_by.end());

		int off_thread = 0;
		for (const std::thread::id& id : drawn_by)
		{
			if (id != std::this_thread::get_id())
			{
				off_thread++;
			}
		}

		char message[256];
		std::snprintf(message, sizeof(message),
			"note    the fan-out drew %d views on %d distinct thread(s) this "
			"run, %d of them off the calling thread",
			view_count, static_cast<int>(distinct.size()), off_thread);
		return message;
	}
}

std::string run_fanout_benchmarks()
{
	Harness harness;

	// One worker per pane at the widest. min_threads is the shell's default
	// and max_threads is a property of the machine there
	// (engine/app/application.h); here the layout is fixed at four panes, so
	// four slices is the most the partitioner can be asked for and a fifth
	// thread would never be given anything.
	ThreadPool pool(1, view_count);
	const Partitioner partitioner;

	// The counts scene_bench.cpp uses, so the two rows below sit under its
	// "render cull (one view)" row at the same problem sizes and can be read
	// against it.
	const int counts[] = { 64, 256, 1024, 4096 };

	for (int count : counts)
	{
		const std::unique_ptr<Scene> serial =
			build_scene(count, harness.quad, nullptr, nullptr);
		const std::unique_ptr<Scene> parallel =
			build_scene(count, harness.quad, &pool, &partitioner);

		Renderer& renderer = harness.renderer();

		// begin_frame is the setup rather than part of the body: it resets
		// every view's recording, which is what makes each repetition measure
		// the same work instead of an ever-growing vector, and bench.h does
		// not count it.
		bench::record(bench::run("Scene::draw (4 views, serial)", count,
			[&serial, &renderer] { serial->draw(renderer); },
			[&renderer] { renderer.begin_frame(); }));

		bench::record(bench::run("Scene::draw (4 views, fan-out)", count,
			[&parallel, &renderer] { parallel->draw(renderer); },
			[&renderer] { renderer.begin_frame(); }));
	}

	return observe_threads(harness, partitioner, pool);
}

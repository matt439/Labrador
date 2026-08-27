#include <doctest/doctest.h>

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

#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

// What the per-view render fan-out owes, asserted for the first time.
//
// PHILOSOPHY commits to one axis of parallelism "and that one alone" and
// CLAUDE.md calls the const draw() discipline guarding it "load-bearing, not
// a convenience" - and until this file the fan-out had never run. scene.cpp
// returns before it when the view count is one, or the pool is null, or the
// partitioner is null; both samples construct Scene(nullptr, nullptr); and
// scene_tests.cpp beside this one builds every scene the same way. So the
// early-out was the only branch ever taken, and the guarantee draw()'s
// constness exists to provide had never been asked for.
// docs/survey/2026-08-26.md 2.1 is the finding; bench/fanout_bench_null.cpp
// measures what the path costs and this file pins what it must produce.
//
// COMPILED ONLY IN THE null CONFIGURATION. Scene::draw takes a Renderer and
// the null backend is the only one with a Renderer that needs no window and no
// adapter, so tests/scene/CMakeLists.txt adds this file to SceneTests there and
// nowhere else - the same rule tests/render/CMakeLists.txt states for
// null_tests.cpp. Everything else in SceneTests is configuration-independent
// and stays that way.
//
// NOTHING HERE ASSERTS ON PARALLELISM, and that is deliberate for the reason
// tests/core/thread_pool_tests.cpp already gives: "A pool with a maximum of
// four threads is allowed to run four tasks on one thread, and a test that
// demanded otherwise would fail on a machine that decided differently." What
// is contractual is that a fan-out produces the frame the single-threaded path
// produces, down to the order, and that a failure inside a worker reaches the
// caller rather than the process. Those are what is asserted, and both are
// deterministic. Whether more than one thread was involved on a given run is
// an observation, and the benchmark reports it as one.

namespace
{
	using namespace labrador;
	using namespace mattmath;

	// A renderer with no graphics API, and a texture built without a file, in
	// the shape tests/render/renderer_seam_tests.cpp builds one: two texels is
	// all a recording needs, and content beside the executable would be a way
	// for this file to fail for a reason that is not about the fan-out.
	class Harness
	{
	public:
		explicit Harness(int view_capacity)
		{
			this->renderer_.create_device(nullptr, 1280, 720, view_capacity);
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

		// A COPY, WHERE THE ENGINE HANDS BACK A REFERENCE, for the reason
		// null_tests.cpp gives: the recording is valid until the next
		// begin_frame, which is the right contract for a frame path and the
		// wrong one for a test comparing two frames.
		std::vector<RecordedSprite> draw(const Scene& scene)
		{
			this->renderer_.begin_frame();
			scene.draw(this->renderer_);
			this->renderer_.submit();
			return recorded_sprites(this->renderer_);
		}

		Renderer& renderer() { return this->renderer_; }

		TextureHandle quad;

	private:
		// The table before the renderer, so it dies after one -
		// render_resources.h states that ordering as a term of the seam.
		RenderResources resources_;
		Renderer renderer_;
	};

	class Block final : public GameObject
	{
	public:
		Block(const RectangleF& rectangle, TextureHandle texture) :
			rectangle_(rectangle), texture_(texture)
		{
		}

		void update(float /*dt*/) override {}

		// A pure read, which is what lets several workers enter it on this
		// same object at once.
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

	class ThrowingBlock final : public GameObject
	{
	public:
		void update(float /*dt*/) override {}

		void draw(DrawList& /*list*/) const override
		{
			throw std::runtime_error("drawn from a worker");
		}

		RectangleF bounds() const override
		{
			return RectangleF(0.0f, 0.0f, 1000.0f, 1000.0f);
		}
	};

	constexpr float cell = 64.0f;
	constexpr float block_size = 48.0f;

	// One arena in a grid, seen by `views` panes side by side, each framing an
	// equal slice of it. The slices are horizontal so that any view count
	// works, including the ones that do not divide the pool width evenly.
	std::unique_ptr<Scene> build_scene(int blocks, int views,
		TextureHandle texture, ThreadPool* pool, const Partitioner* partitioner)
	{
		std::unique_ptr<Scene> scene =
			std::make_unique<Scene>(pool, partitioner);

		int side = 1;
		while (side * side < blocks)
		{
			side++;
		}

		for (int i = 0; i < blocks; i++)
		{
			const RectangleF place(static_cast<float>(i % side) * cell,
				static_cast<float>(i / side) * cell, block_size, block_size);
			scene->add(std::make_unique<Block>(place, texture));
		}
		scene->end_tick();

		const float world = static_cast<float>(side) * cell;
		const float slice = world / static_cast<float>(views);

		for (int i = 0; i < views; i++)
		{
			const Viewport viewport(0.0f, 0.0f, 320.0f, 240.0f);
			const RectangleF seen(static_cast<float>(i) * slice, 0.0f,
				slice, world);
			scene->add_view(viewport, Camera::frame(seen, viewport));
		}

		return scene;
	}

	// Every term of a recorded sprite, because a fan-out that lost one of them
	// would be a fan-out that drew the right sprites into the wrong place.
	bool same(const RecordedSprite& left, const RecordedSprite& right)
	{
		if (left.view != right.view ||
			left.texture.index() != right.texture.index() ||
			left.filter != right.filter ||
			left.viewport.rectangle() != right.viewport.rectangle())
		{
			return false;
		}

		for (int i = 0; i < 4; i++)
		{
			if (left.corners[i].position != right.corners[i].position ||
				left.corners[i].texcoord != right.corners[i].texcoord ||
				left.corners[i].colour != right.corners[i].colour)
			{
				return false;
			}
		}
		return true;
	}

	// The index of the first sprite the two frames disagree about, or the size
	// when they do not disagree at all.
	//
	// ONE ASSERTION AND NOT ONE PER SPRITE. A CHECK inside the loop would put
	// a couple of thousand assertions into every run of SceneTests and, on a
	// failure, print every one of them - where what a reader needs is which
	// sprite diverged first, which is the number doctest prints when this is
	// compared against the size.
	size_t first_difference(const std::vector<RecordedSprite>& expected,
		const std::vector<RecordedSprite>& actual)
	{
		for (size_t i = 0; i < expected.size() && i < actual.size(); i++)
		{
			if (!same(expected[i], actual[i]))
			{
				return i;
			}
		}
		return expected.size();
	}
}

TEST_CASE("CONTRACT: the fan-out records what the single-threaded path records")
{
	Harness harness(4);
	ThreadPool pool(1, 4);
	const Partitioner partitioner;

	const std::unique_ptr<Scene> serial =
		build_scene(400, 4, harness.quad, nullptr, nullptr);
	const std::unique_ptr<Scene> parallel =
		build_scene(400, 4, harness.quad, &pool, &partitioner);

	const std::vector<RecordedSprite> expected = harness.draw(*serial);
	const std::vector<RecordedSprite> actual = harness.draw(*parallel);

	// A frame that recorded nothing would pass every comparison below and
	// prove none of them.
	REQUIRE(expected.size() > 0);
	REQUIRE(actual.size() == expected.size());

	// IN ORDER, NOT MERELY AS A SET. renderer.h makes view order the only
	// ordering guarantee the seam offers, and call order within a view; a
	// fan-out that let two workers interleave into one recording would still
	// hold the same sprites.
	CHECK(first_difference(expected, actual) == expected.size());
}

TEST_CASE("the fan-out draws every view when the slices do not divide evenly")
{
	// Three views over a pool four wide, so the partitioner cuts three ranges
	// out of a width that would rather have four - the remainder path
	// Partitioner::partition spreads one element at a time, reached through
	// Scene::draw for the first time here.
	Harness harness(4);
	ThreadPool pool(1, 4);
	const Partitioner partitioner;

	const std::unique_ptr<Scene> serial =
		build_scene(120, 3, harness.quad, nullptr, nullptr);
	const std::unique_ptr<Scene> parallel =
		build_scene(120, 3, harness.quad, &pool, &partitioner);

	const std::vector<RecordedSprite> expected = harness.draw(*serial);
	const std::vector<RecordedSprite> actual = harness.draw(*parallel);

	REQUIRE(actual.size() == expected.size());
	CHECK(first_difference(expected, actual) == expected.size());

	// And all three of them are in it, which is the half of "every view
	// exactly once" that a comparison against the serial path cannot catch:
	// a partitioner dropping the same range on both would agree with itself.
	std::set<int> views_drawn;
	for (const RecordedSprite& sprite : actual)
	{
		views_drawn.insert(sprite.view);
	}

	CHECK(views_drawn.size() == 3u);
	CHECK(views_drawn.count(0) == 1u);
	CHECK(views_drawn.count(1) == 1u);
	CHECK(views_drawn.count(2) == 1u);
}

TEST_CASE("CONTRACT: a throw inside a worker reaches the thread that drew")
{
	// The one behaviour of the parallel path the serial path cannot have,
	// and the one that decides whether a bug in a client's draw() is a
	// diagnosable exception or a process that vanished. ThreadPool promises
	// it and tests/core/thread_pool_tests.cpp pins it there; this is the
	// promise arriving through Scene::draw, which is the only caller.
	Harness harness(4);
	ThreadPool pool(1, 4);
	const Partitioner partitioner;

	const std::unique_ptr<Scene> scene =
		build_scene(4, 4, harness.quad, &pool, &partitioner);
	scene->add(std::make_unique<ThrowingBlock>());
	scene->end_tick();

	harness.renderer().begin_frame();
	CHECK_THROWS_AS(scene->draw(harness.renderer()), std::runtime_error);
}

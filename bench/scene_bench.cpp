#include "bench/bench.h"

#include "engine/collision/collision_layer.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/scene/scene.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <vector>

using labrador::CollisionLayer;
using labrador::CollisionMask;
using labrador::CollisionObject;
using labrador::CollisionTag;
using labrador::Contact;
using labrador::GameObject;
using labrador::Scene;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	// A representative scene, in the sense PHILOSOPHY means: the shape of a
	// shipped level, scaled up.
	//
	// The three shipped levels hold 18, 33 and 34 collision objects plus six
	// visuals each, and up to four players with their projectiles - so a real
	// match is around fifty objects. That is a useful data point and a useless
	// benchmark: everything is fast at fifty. The counts below run past it by
	// three orders of magnitude, because what is being measured is which phase
	// stops scaling first, and at fifty the answer is none of them.
	constexpr CollisionLayer WORLD = 1u << 0;
	constexpr CollisionLayer MOVER = 1u << 1;

	// The world is a grid, which is what a paint arena is: structures laid out
	// in a plane, mostly not touching. A random scatter would measure the
	// random number generator's cache behaviour as much as the sweep's.
	constexpr float CELL = 64.0f;
	constexpr float SIZE = 48.0f;

	class BenchObject final : public GameObject
	{
	public:
		explicit BenchObject(const RectangleF& rectangle) :
			rectangle_(rectangle)
		{
		}

		void update(float dt) override { this->age_ += dt; }
		void draw(labrador::DrawList& /*list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

	private:
		RectangleF rectangle_;
		float age_ = 0.0f;
	};

	class BenchCollider final : public CollisionObject
	{
	public:
		BenchCollider(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask) :
			rectangle_(rectangle), layer_(layer), mask_(mask)
		{
		}

		void update(float dt) override { this->age_ += dt; }
		void draw(labrador::DrawList& /*list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const mattmath::Shape* shape() const override
		{
			return &this->rectangle_;
		}
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return 0; }

		// Counted rather than acted on. Separating here would move the
		// rectangles and change what the next repetition measures, and what is
		// being timed is the sweep that finds the contacts, not a game's
		// response to them.
		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
			this->contacts_++;
		}

		bool for_deletion() const override { return false; }

	private:
		RectangleF rectangle_;
		int contacts_ = 0;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
		float age_ = 0.0f;
	};

	int side_of(int count)
	{
		int side = 1;
		while (side * side < count)
		{
			side++;
		}
		return side;
	}

	RectangleF cell_at(int index, int side)
	{
		const float x = static_cast<float>(index % side) * CELL;
		const float y = static_cast<float>(index / side) * CELL;
		return RectangleF(x, y, SIZE, SIZE);
	}

	// Half world geometry and half movers, so the layer filter rejects some
	// pairs and accepts others - a sweep where everything is filtered out
	// measures the filter and nothing else.
	std::unique_ptr<Scene> build_scene(int collider_count, int object_count)
	{
		auto scene = std::make_unique<Scene>(nullptr, nullptr);

		const int side = side_of(collider_count);
		for (int i = 0; i < collider_count; i++)
		{
			const bool mover = (i % 2) == 1;
			scene->add(std::make_unique<BenchCollider>(cell_at(i, side),
				mover ? MOVER : WORLD,
				mover ? (WORLD | MOVER) : MOVER));
		}

		const int object_side = side_of(object_count);
		for (int i = 0; i < object_count; i++)
		{
			scene->add(std::make_unique<BenchObject>(cell_at(i, object_side)));
		}

		scene->end_tick();
		return scene;
	}

	// The render cull, lifted out of Scene::draw_views.
	//
	// It is duplicated here rather than driven through Scene::draw because
	// draw needs a Renderer. The null backend now supplies one with no device
	// (engine/render/null/), but only in the configuration that builds it, and
	// this benchmark builds in all five - so driving it through scene.draw()
	// would measure a different thing depending on which preset ran it, which
	// is worse than a duplicated loop. The loop is the two lines that matter -
	// bounds() against the visible rectangle - and measuring it separately is
	// honest as long as this comment says so.
	//
	// THE REAL THING IS MEASURED NOW, NEXT DOOR AND NOT HERE.
	// bench/fanout_bench_null.cpp drives Scene::draw itself, both branches of
	// it, in the one configuration that can - which is the case this stand-in
	// could never be, and it is also why this one stays: the two rows it adds
	// are absent from four presets out of five, and these four phases are not.
	int cull_count(const Scene& scene, const RectangleF& visible)
	{
		int drawn = 0;
		for (CollisionObject* const object : scene.collision_objects())
		{
			if (object->bounds().intersects(visible))
			{
				drawn++;
			}
		}
		return drawn;
	}
}

void run_scene_benchmarks()
{
	// 64 is around the shipped scale; the rest are the three orders of
	// magnitude past it that make the curve visible.
	const int counts[] = { 64, 256, 1024, 4096 };

	for (int count : counts)
	{
		std::unique_ptr<Scene> scene = build_scene(count, count);
		std::vector<Contact> swept;

		bench::record(bench::run("Scene::update", count,
			[&scene] { scene->update(1.0f / 60.0f); }));

		// The scene's own resolve, which uses the broad phase.
		bench::record(bench::run("Scene::resolve (broad phase)", count,
			[&scene] { scene->resolve(); }));

		// The same work with the grid taken away, which is what the sweep
		// cost before it existed and what the comparison is against.
		bench::record(bench::run("Scene::resolve (n^2 sweep)", count,
			[&scene, &swept] {
				find_contacts(scene->collision_objects(), swept);
			}));

		bench::record(bench::run("Scene::end_tick", count,
			[&scene] { scene->end_tick(); }));

		// A player's pane on a 1280x720 four-way split, over a world that is
		// much larger - so the cull rejects nearly everything, which is the
		// case it exists for.
		const RectangleF visible(0.0f, 0.0f, 640.0f, 360.0f);
		bench::record(bench::run("render cull (one view)", count,
			[&scene, &visible] {
				volatile int drawn = cull_count(*scene, visible);
				(void)drawn;
			}));
	}
}

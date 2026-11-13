#include "bench/bench.h"

#include "engine/collision/collision_layer.h"
#include "engine/collision/collision_object.h"
#include "engine/collision/contacts.h"
#include "engine/scene/scene.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <span>
#include <vector>

using labrador::CollisionLayer;
using labrador::CollisionMask;
using labrador::CollisionObject;
using labrador::CollisionTag;
using labrador::Contact;
using labrador::GameObject;
using labrador::Scene;
using labrador::RectangleF;
using labrador::Vector2F;

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
	constexpr CollisionLayer world_layer = 1u << 0;
	constexpr CollisionLayer mover_layer = 1u << 1;

	// The world is a grid, which is what a paint arena is: structures laid out
	// in a plane, mostly not touching. A random scatter would measure the
	// random number generator's cache behaviour as much as the sweep's.
	constexpr float cell = 64.0f;
	constexpr float size = 48.0f;

	class BenchObject final : public GameObject
	{
	public:
		explicit BenchObject(const RectangleF& rectangle) :
			rectangle_(rectangle)
		{
		}

		void update(float /*dt*/) override {}
		void draw(labrador::DrawList& /*list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

	private:
		RectangleF rectangle_;
	};

	class BenchCollider final : public CollisionObject
	{
	public:
		BenchCollider(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask) :
			rectangle_(rectangle), layer_(layer), mask_(mask)
		{
		}

		void update(float /*dt*/) override {}
		void draw(labrador::DrawList& /*list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const labrador::Shape* shape() const override
		{
			return &this->rectangle_;
		}
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return 0; }

		// Ignored rather than acted on. Separating here would move the
		// rectangles and change what the next repetition measures, and what is
		// being timed is the sweep that finds the contacts, not a game's
		// response to them.
		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
		}

		bool for_deletion() const override { return false; }

	private:
		RectangleF rectangle_;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
	};

	// A collider that records set_for_deletion, which BenchCollider's
	// fixed-geometry default does not: what the churn case retires.
	class ChurnCollider final : public CollisionObject
	{
	public:
		ChurnCollider(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask) :
			rectangle_(rectangle), layer_(layer), mask_(mask)
		{
		}

		void update(float /*dt*/) override {}
		void draw(labrador::DrawList& /*list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const labrador::Shape* shape() const override
		{
			return &this->rectangle_;
		}
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return 0; }

		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
		}

		bool for_deletion() const override { return this->for_deletion_; }
		void set_for_deletion(bool for_deletion) override
		{
			this->for_deletion_ = for_deletion;
		}

	private:
		RectangleF rectangle_;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
		bool for_deletion_ = false;
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
		const float x = static_cast<float>(index % side) * cell;
		const float y = static_cast<float>(index / side) * cell;
		return RectangleF(x, y, size, size);
	}

	struct BenchScene
	{
		std::unique_ptr<Scene> scene;

		// The plain objects in insertion order. Scene keeps its own list of
		// them private, and the cull below walks them as Scene::draw_views
		// does.
		std::vector<GameObject*> objects;
	};

	// Half world geometry and half movers, so the layer filter rejects some
	// pairs and accepts others - a sweep where everything is filtered out
	// measures the filter and nothing else.
	BenchScene build_scene(int collider_count, int object_count)
	{
		BenchScene built;
		built.scene = std::make_unique<Scene>(nullptr, nullptr);

		const int side = side_of(collider_count);
		for (int i = 0; i < collider_count; i++)
		{
			const bool mover = (i % 2) == 1;
			built.scene->add(std::make_unique<BenchCollider>(cell_at(i, side),
				mover ? mover_layer : world_layer,
				mover ? (world_layer | mover_layer) : mover_layer));
		}

		const int object_side = side_of(object_count);
		built.objects.reserve(static_cast<size_t>(object_count));
		for (int i = 0; i < object_count; i++)
		{
			built.objects.push_back(built.scene->add(
				std::make_unique<BenchObject>(cell_at(i, object_side))));
		}

		built.scene->end_tick();
		return built;
	}

	// The churn world: `count` colliders on the same grid and with the same
	// half-and-half layers as build_scene, but each one able to be retired.
	std::unique_ptr<Scene> build_churn_scene(int count)
	{
		std::unique_ptr<Scene> scene = std::make_unique<Scene>(nullptr, nullptr);

		const int side = side_of(count);
		for (int i = 0; i < count; i++)
		{
			const bool mover = (i % 2) == 1;
			scene->add(std::make_unique<ChurnCollider>(cell_at(i, side),
				mover ? mover_layer : world_layer,
				mover ? (world_layer | mover_layer) : mover_layer));
		}

		scene->end_tick();
		return scene;
	}

	// One whole tick of a world whose collidables come and go: the oldest
	// `retired` are flagged for deletion and as many are spawned in their
	// places, then the scene runs update, resolve and end_tick.
	//
	// Each collidable is one registered object carrying one Shape, so a world
	// whose collidables churn - projectiles, debris, pickups - spawns and
	// retires that many objects a tick, where a world of plain values churns
	// inside one object as LineSweeper's particles do. Retirement through
	// for_deletion() is the path for it: end_tick's erasure is one pass.
	// Scene::remove is not, because it walks the scene to check its argument.
	//
	// The oldest go because survivors keep their order and spawns join the
	// end, so the scene is a rolling queue: every tick retires objects the
	// previous ticks spawned, and the count and the geometry hold steady
	// across repetitions without a setup step.
	void churn_tick(Scene& scene, int retired)
	{
		const std::span<CollisionObject* const> live = scene.collision_objects();
		for (int i = 0; i < retired; i++)
		{
			CollisionObject* const object = live[static_cast<size_t>(i)];
			object->set_for_deletion(true);
			scene.add(std::make_unique<ChurnCollider>(object->bounds(),
				object->layer(), object->mask()));
		}

		scene.update(1.0f / 60.0f);
		scene.resolve();
		scene.end_tick();
	}

	// The world units per pixel of a camera at scale 1, which is what
	// Scene::draw_views passes to cull_bounds as 1 / camera.scale.
	constexpr float units_per_pixel = 1.0f;

	// The per-view cull of Scene::draw_views (engine/scene/scene.cpp),
	// copied: the plain objects and then the collision objects, each one's
	// cull_bounds at the view's world units per pixel tested against the
	// visible rectangle. A change to that loop changes this copy with it.
	//
	// Copied rather than driven through Scene::draw because draw needs a
	// Renderer, and only the null backend has one without a window and a
	// device, while this benchmark builds under all five. Driving draw would
	// measure a different thing depending on which preset ran it.
	// bench/fanout_bench_null.cpp drives Scene::draw itself in the null
	// configuration; this row is the cull every configuration measures.
	int cull_count(const BenchScene& built, const RectangleF& visible)
	{
		int drawn = 0;
		for (GameObject* const object : built.objects)
		{
			if (object->cull_bounds(units_per_pixel).intersects(visible))
			{
				drawn++;
			}
		}

		for (CollisionObject* const object : built.scene->collision_objects())
		{
			if (object->cull_bounds(units_per_pixel).intersects(visible))
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
		const BenchScene built = build_scene(count, count);
		const std::unique_ptr<Scene>& scene = built.scene;
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
			[&built, &visible] {
				volatile int drawn = cull_count(built, visible);
				(void)drawn;
			}));

		// An eighth of the colliders replaced every tick. The fraction is
		// fixed so the work per tick grows with the count; what the case
		// guards is a spawn or a retirement whose cost depends on how many
		// objects the scene already holds.
		const std::unique_ptr<Scene> churn = build_churn_scene(count);
		const int retired = count / 8;
		bench::record(bench::run("Scene tick (1/8 spawned and retired)", count,
			[&churn, retired] { churn_tick(*churn, retired); }));
	}
}

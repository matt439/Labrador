#include <doctest/doctest.h>

#include "engine/scene/scene.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <memory>
#include <stdexcept>
#include <vector>

using labrador::CollisionLayer;
using labrador::CollisionMask;
using labrador::CollisionObject;
using labrador::CollisionTag;
using labrador::GameObject;
using labrador::Scene;
using labrador::Camera;
using mattmath::RectangleF;
using mattmath::Vector2F;
using labrador::Viewport;

namespace
{
	constexpr CollisionLayer PLAYER = 1u << 0;
	constexpr CollisionLayer WALL = 1u << 1;

	// Counts the ticks it was stepped for, which is all the scene does to a
	// plain object.
	class TestObject : public GameObject
	{
	public:
		explicit TestObject(const RectangleF& rectangle) :
			rectangle_(rectangle)
		{
		}

		void update(float dt) override
		{
			this->updates_++;
			this->last_dt_ = dt;
		}

		void draw(labrador::DrawList& /*draw_list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		int updates() const { return this->updates_; }
		float last_dt() const { return this->last_dt_; }

	private:
		RectangleF rectangle_;
		int updates_ = 0;
		float last_dt_ = 0.0f;
	};

	class TestCollider : public CollisionObject
	{
	public:
		TestCollider(const RectangleF& rectangle, CollisionLayer layer,
			CollisionMask mask, CollisionTag tag = 0) :
			rectangle_(rectangle), layer_(layer), mask_(mask), tag_(tag)
		{
		}

		void update(float /*dt*/) override { this->updates_++; }
		void draw(labrador::DrawList& /*draw_list*/) const override {}
		RectangleF bounds() const override { return this->rectangle_; }

		const mattmath::Shape* shape() const override
		{
			return &this->rectangle_;
		}
		CollisionLayer layer() const override { return this->layer_; }
		CollisionMask mask() const override { return this->mask_; }
		CollisionTag tag() const override { return this->tag_; }

		bool for_deletion() const override { return this->for_deletion_; }
		void set_for_deletion(bool for_deletion) override
		{
			if (this->fixed_)
			{
				return;
			}
			this->for_deletion_ = for_deletion;
		}

		void on_contact(const CollisionObject& other,
			const Vector2F& /*normal*/, float /*penetration*/) override
		{
			this->contacts_++;
			this->last_other_ = &other;
		}

		int updates() const { return this->updates_; }
		int contacts() const { return this->contacts_; }
		const CollisionObject* last_other() const { return this->last_other_; }

		// Level geometry: it cannot be removed and ignores the request, which is
		// the default CollisionObject::set_for_deletion documents.
		void make_fixed() { this->fixed_ = true; }

	private:
		RectangleF rectangle_;
		CollisionLayer layer_ = 0;
		CollisionMask mask_ = 0;
		CollisionTag tag_ = 0;
		bool for_deletion_ = false;
		bool fixed_ = false;
		int updates_ = 0;
		int contacts_ = 0;
		const CollisionObject* last_other_ = nullptr;
	};

	// No thread pool and no partitioner. Everything below the draw is
	// single-threaded anyway, and draw() itself needs a Renderer - see the
	// closing comment in this file.
	Scene make_scene() { return Scene(nullptr, nullptr); }

	bool holds(const Scene& scene, const CollisionObject* object)
	{
		for (const CollisionObject* candidate : scene.collision_objects())
		{
			if (candidate == object)
			{
				return true;
			}
		}
		return false;
	}
}

TEST_CASE("add() returns the object's own type, and defers the insertion")
{
	Scene scene = make_scene();

	TestCollider* collider = scene.add(std::make_unique<TestCollider>(
		RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL));

	// A pointer of the derived type, valid the moment add() returns - which is
	// what a caller that has to keep speaking to the object needs.
	REQUIRE(collider != nullptr);
	CHECK(collider->bounds().width == doctest::Approx(10.0f));

	// And not in the world yet.
	CHECK(scene.collision_objects().empty());

	scene.end_tick();

	CHECK(scene.collision_objects().size() == 1);
	CHECK(holds(scene, collider));
}

TEST_CASE("update() steps every object in the world, and nothing pending")
{
	Scene scene = make_scene();

	TestObject* plain = scene.add(std::make_unique<TestObject>(
		RectangleF(0.0f, 0.0f, 4.0f, 4.0f)));
	TestCollider* collider = scene.add(std::make_unique<TestCollider>(
		RectangleF(0.0f, 0.0f, 4.0f, 4.0f), PLAYER, WALL));

	// Nothing has been admitted, so nothing steps.
	scene.update(0.5f);
	CHECK(plain->updates() == 0);
	CHECK(collider->updates() == 0);

	scene.end_tick();

	scene.update(0.25f);
	CHECK(plain->updates() == 1);
	CHECK(plain->last_dt() == doctest::Approx(0.25f));
	CHECK(collider->updates() == 1);
}

TEST_CASE("resolve() measures the pairs in the world and tells both sides")
{
	Scene scene = make_scene();

	TestCollider* player = scene.add(std::make_unique<TestCollider>(
		RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL));
	TestCollider* wall = scene.add(std::make_unique<TestCollider>(
		RectangleF(8.0f, 0.0f, 10.0f, 10.0f), WALL, PLAYER));
	scene.end_tick();

	scene.resolve();

	REQUIRE(scene.contacts().size() == 1);
	CHECK(player->contacts() == 1);
	CHECK(wall->contacts() == 1);
	CHECK(player->last_other() == wall);
	CHECK(wall->last_other() == player);
}

TEST_CASE("an object added mid-tick is swept by the next tick, not this one")
{
	// This is what the deferral is for, and it is not tidiness: the players and
	// their projectiles are one list, so a push from inside the loop walking it
	// would invalidate the walk.
	Scene scene = make_scene();

	TestCollider* wall = scene.add(std::make_unique<TestCollider>(
		RectangleF(0.0f, 0.0f, 10.0f, 10.0f), WALL, PLAYER));
	scene.end_tick();

	// A weapon fires, on top of the wall.
	TestCollider* bullet = scene.add(std::make_unique<TestCollider>(
		RectangleF(2.0f, 2.0f, 2.0f, 2.0f), PLAYER, WALL));

	scene.resolve();
	CHECK(scene.contacts().empty());
	CHECK(wall->contacts() == 0);
	CHECK(bullet->contacts() == 0);

	scene.end_tick();
	scene.resolve();

	CHECK(scene.contacts().size() == 1);
	CHECK(bullet->contacts() == 1);
}

TEST_CASE("end_tick() retires before it admits")
{
	Scene scene = make_scene();

	TestCollider* doomed = scene.add(std::make_unique<TestCollider>(
		RectangleF(0.0f, 0.0f, 10.0f, 10.0f), PLAYER, WALL));
	scene.end_tick();
	REQUIRE(scene.collision_objects().size() == 1);

	doomed->set_for_deletion(true);
	TestCollider* fresh = scene.add(std::make_unique<TestCollider>(
		RectangleF(100.0f, 100.0f, 10.0f, 10.0f), PLAYER, WALL));

	scene.end_tick();

	// The new object cannot have been caught by the retirement pass that ran
	// before it existed, and the flagged one is gone.
	REQUIRE(scene.collision_objects().size() == 1);
	CHECK(holds(scene, fresh));
}

TEST_CASE("the world's bounds retire what leaves them")
{
	Scene scene = make_scene();
	scene.set_bounds(RectangleF(0.0f, 0.0f, 100.0f, 100.0f));

	TestCollider* inside = scene.add(std::make_unique<TestCollider>(
		RectangleF(10.0f, 10.0f, 5.0f, 5.0f), PLAYER, WALL));
	TestCollider* outside = scene.add(std::make_unique<TestCollider>(
		RectangleF(500.0f, 500.0f, 5.0f, 5.0f), PLAYER, WALL));

	// The pending pass admits both, and the retirement pass ran before them, so
	// the one out of bounds survives its first end_tick and dies on its second.
	scene.end_tick();
	CHECK(scene.collision_objects().size() == 2);

	scene.end_tick();

	REQUIRE(scene.collision_objects().size() == 1);
	CHECK(holds(scene, inside));
	CHECK(!holds(scene, outside));
}

TEST_CASE("bounds are optional, and an object that refuses removal keeps it")
{
	Scene unbounded = make_scene();
	TestCollider* far_away = unbounded.add(std::make_unique<TestCollider>(
		RectangleF(-9000.0f, -9000.0f, 5.0f, 5.0f), PLAYER, WALL));
	unbounded.end_tick();
	unbounded.end_tick();

	CHECK(!unbounded.bounds().has_value());
	CHECK(unbounded.in_bounds(*far_away));
	CHECK(unbounded.collision_objects().size() == 1);

	// Fixed geometry outside the bounds: the scene asks, the object declines,
	// and nothing is silently deleted out from under the level.
	Scene bounded = make_scene();
	bounded.set_bounds(RectangleF(0.0f, 0.0f, 100.0f, 100.0f));
	TestCollider* fixed = bounded.add(std::make_unique<TestCollider>(
		RectangleF(500.0f, 500.0f, 5.0f, 5.0f), WALL, PLAYER));
	fixed->make_fixed();
	bounded.end_tick();
	bounded.end_tick();

	CHECK(!bounded.in_bounds(*fixed));
	CHECK(bounded.collision_objects().size() == 1);
}

TEST_CASE("the view list is the game's, and it is refilled not appended to")
{
	Scene scene = make_scene();

	CHECK(scene.view_count() == 0);

	scene.add_view(Viewport(0.0f, 0.0f, 640.0f, 360.0f),
		Camera(Vector2F(100.0f, 50.0f), 2.0f));
	scene.add_view(Viewport(640.0f, 0.0f, 640.0f, 360.0f));

	REQUIRE(scene.view_count() == 2);
	CHECK(scene.view(0).viewport.width == doctest::Approx(640.0f));
	CHECK(scene.view(0).camera.scale == doctest::Approx(2.0f));

	// The default is the identity, so a caller that only ever wanted screen
	// space never mentions a camera.
	CHECK(scene.view(1).camera == Camera::DEFAULT_CAMERA);

	CHECK_THROWS_AS(scene.view(2), std::out_of_range);
	CHECK_THROWS_AS(scene.view(-1), std::out_of_range);

	scene.clear_views();
	CHECK(scene.view_count() == 0);
}

// NOT TESTED HERE, and this paragraph has outlived both of the reasons it gave.
// It read "the same gap engine/render/renderer.h closes with STILL OPEN", and
// that header has no open question left to close one with; and it read "the
// only Renderer that exists is the D3D11 one", and there are four.
//
// What is still true is that Scene::draw needs a Renderer, and that the cull it
// performs - bounds() against the camera's translation and the pane's size over
// its zoom - is the arithmetic that wants a test. What is no longer true is the
// promise this made of one "the day a null backend lands in
// engine/render/null/". That day came. A case that draws would be built in one
// configuration out of four, the way tests/render/null_tests.cpp is, and this
// target is built in all five - so the test is owed rather than blocked, which
// is a different sentence from the one that was here.

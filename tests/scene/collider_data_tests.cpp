#include <doctest/doctest.h>

#include "engine/scene/scene.h"
#include "tests/collision/stub_collider.h"

#include <array>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace labrador;

namespace
{
	class MutableCollider final : public StubCollider
	{
	public:
		explicit MutableCollider(const RectangleF& rectangle) :
			StubCollider(rectangle, 1, 1, 91) {}

		void set_rectangle(const RectangleF& rectangle) { this->rectangle_ = rectangle; }
		void on_contact(const CollisionObject&, const Vector2F&, float) override
		{
			++this->received;
		}

		int received = 0;
	};
}

TEST_CASE("scene boundaries refuse each nonfinite collider field with or without world bounds")
{
	const std::array<float RectangleF::*, 4> fields = {
		&RectangleF::x, &RectangleF::y, &RectangleF::width, &RectangleF::height };
	for (const bool bounded : { false, true })
	{
		for (float RectangleF::* field : fields)
		{
			for (const float poison : { std::numeric_limits<float>::quiet_NaN(),
				std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
			{
				CAPTURE(bounded);
				CAPTURE(poison);
				Scene scene(nullptr, nullptr);
				if (bounded) scene.set_bounds(RectangleF(-100, -100, 200, 200));
				const RectangleF good(0, 0, 10, 10);
				RectangleF invalid = good;
				invalid.*field = poison;
				MutableCollider* object = scene.add(std::make_unique<MutableCollider>(invalid));
				CHECK(scene.collision_objects().empty());
				CHECK_THROWS_AS(scene.in_bounds(*object), std::invalid_argument);
				CHECK_THROWS_AS(scene.end_tick(), std::invalid_argument);
				CHECK(scene.collision_objects().empty());
				CHECK_FALSE(object->for_deletion());

				// The failed admission leaves the borrowed pointer and ownership
				// intact; the caller repairs it and retries the same pending add.
				object->set_rectangle(good);
				CHECK_NOTHROW(scene.end_tick());
				REQUIRE(scene.collision_objects().size() == 1);
				CHECK(scene.collision_objects()[0] == object);
				CHECK(scene.in_bounds(*object));

				object->set_rectangle(invalid);
				CHECK_THROWS_AS(scene.resolve(), std::invalid_argument);
				CHECK(scene.contacts().empty());
				CHECK(object->received == 0);
				CHECK_THROWS_AS(scene.end_tick(), std::invalid_argument);
				REQUIRE(scene.collision_objects().size() == 1);
				CHECK_FALSE(object->for_deletion());
				CHECK_NOTHROW(scene.remove(object));
				CHECK_NOTHROW(scene.end_tick());
				CHECK(scene.collision_objects().empty());
			}
		}
	}
}

TEST_CASE("failed end_tick expires contacts before any bounds retirement or pending admission")
{
	Scene scene(nullptr, nullptr);
	scene.set_bounds(RectangleF(-100, -100, 200, 200));
	MutableCollider* first = scene.add(std::make_unique<MutableCollider>(RectangleF(0, 0, 10, 10)));
	MutableCollider* second = scene.add(std::make_unique<MutableCollider>(RectangleF(2, 2, 10, 10)));
	scene.end_tick();
	scene.resolve();
	REQUIRE(scene.contacts().size() == 1);
	first->set_rectangle(RectangleF(500, 500, 10, 10));
	second->set_rectangle(RectangleF(0, 0, std::numeric_limits<float>::infinity(), 10));
	MutableCollider* pending = scene.add(std::make_unique<MutableCollider>(RectangleF(20, 20, 10, 10)));
	CHECK_THROWS_AS(scene.end_tick(), std::invalid_argument);
	CHECK(scene.contacts().empty());
	CHECK_FALSE(first->for_deletion());
	CHECK_FALSE(second->for_deletion());
	REQUIRE(scene.collision_objects().size() == 2);

	// A queued removal is retained even if another invalid object prevents
	// this end_tick; fixing it then admits the pending object exactly once.
	scene.remove(first);
	CHECK_THROWS_AS(scene.end_tick(), std::invalid_argument);
	second->set_rectangle(RectangleF(0, 0, 10, 10));
	scene.end_tick();
	REQUIRE(scene.collision_objects().size() == 2);
	CHECK(scene.collision_objects()[0] == second);
	CHECK(scene.collision_objects()[1] == pending);
	CHECK_NOTHROW(scene.resolve());
}

TEST_CASE("a pending invalid collider can be explicitly discarded without admission")
{
	Scene scene(nullptr, nullptr);
	MutableCollider* object = scene.add(std::make_unique<MutableCollider>(
		RectangleF(0, std::numeric_limits<float>::quiet_NaN(), 10, 10)));
	CHECK_THROWS_AS(scene.end_tick(), std::invalid_argument);
	scene.remove(object);
	CHECK_NOTHROW(scene.end_tick());
	CHECK(scene.collision_objects().empty());
}

TEST_CASE("already retired invalid data is discarded while valid contacts keep their order")
{
	Scene scene(nullptr, nullptr);
	MutableCollider* retired = scene.add(std::make_unique<MutableCollider>(
		RectangleF(0, std::numeric_limits<float>::quiet_NaN(), 10, 10)));
	retired->set_for_deletion(true);
	MutableCollider* first = scene.add(std::make_unique<MutableCollider>(RectangleF(0, 0, 10, 10)));
	MutableCollider* second = scene.add(std::make_unique<MutableCollider>(RectangleF(2, 2, 10, 10)));
	CHECK_NOTHROW(scene.end_tick());
	CHECK_NOTHROW(scene.resolve());
	REQUIRE(scene.contacts().size() == 1);
	CHECK(scene.contacts()[0].a == first);
	CHECK(scene.contacts()[0].b == second);
	CHECK(first->received == 1);
	CHECK(second->received == 1);
	CHECK_NOTHROW(scene.end_tick());
	CHECK(scene.contacts().empty());
	REQUIRE(scene.collision_objects().size() == 2);
}

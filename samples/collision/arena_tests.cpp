#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "samples/collision/arena.h"
#include "samples/collision/entry_marker.h"

#include "engine/math/circle.h"

#include <cmath>
#include <memory>

using namespace labrador;
using namespace collision;

namespace
{
	constexpr float step_time = 1.0f / 60.0f;

	void move(Arena& arena, const Vector2F& direction, int ticks)
	{
		for (int tick = 0; tick < ticks; ++tick)
		{
			arena.step(direction, step_time);
		}
	}

	// A round post the player treats as a wall. The sample's own bodies are
	// rectangles, because they are drawn from one white texel; this one is
	// only measured, so it needs no drawing.
	class RoundPost final : public CollisionObject
	{
	public:
		explicit RoundPost(const Circle& circle) : circle_(circle) {}

		void update(float /*dt*/) override {}
		void draw(DrawList& /*draw_list*/) const override {}
		RectangleF bounds() const override { return this->circle_.bounding_box(); }
		const Shape* shape() const override { return &this->circle_; }
		CollisionLayer layer() const override { return wall_layer; }
		CollisionMask mask() const override { return player_layer; }
		CollisionTag tag() const override
		{
			return static_cast<CollisionTag>(BodyKind::wall);
		}
		bool for_deletion() const override { return false; }
		void on_contact(const CollisionObject& /*other*/,
			const Vector2F& /*normal*/, float /*penetration*/) override {}

	private:
		Circle circle_;
	};

	void add_post(Arena& arena, const Circle& circle)
	{
		arena.scene().add(std::make_unique<RoundPost>(circle));
		arena.scene().end_tick();
	}
}

TEST_CASE("player separates from a solid wall and keeps tangential velocity")
{
	Arena arena;
	arena.player().set_position(Vector2F(567.0f, 320.0f));
	arena.step(Vector2F(1.0f, 1.0f), step_time);
	CHECK(arena.player().bounds().right() == doctest::Approx(600.0f));
	CHECK(arena.player().bounds().y == doctest::Approx(320.0f + 4.0f / std::sqrt(2.0f)));
	CHECK(arena.player().velocity().x == doctest::Approx(0.0f));
	CHECK(arena.player().velocity().y == doctest::Approx(240.0f / std::sqrt(2.0f)));
	CHECK(arena.player().touching_wall());
	CHECK(arena.contact_count() == 1);
	CHECK(arena.scene().contacts().empty());
	move(arena, Vector2F(1.0f, 0.0f), 100);
	CHECK(arena.player().bounds().right() == doctest::Approx(600.0f));
}

TEST_CASE("a zero mask vetoes a pair even when the player accepts its layer")
{
	Arena arena;
	arena.player().set_position(Vector2F(350.0f, 340.0f));
	arena.step(Vector2F(1.0f, 0.0f), step_time);
	CHECK(arena.player().bounds().x == doctest::Approx(354.0f));
	CHECK(arena.contact_count() == 0);
	CHECK_FALSE(arena.player().touching_wall());
	CHECK_FALSE(arena.player().touching_trigger());
}

TEST_CASE("trigger contacts report entry without separating or repeating each tick")
{
	Arena arena;
	arena.player().set_position(Vector2F(920.0f, 320.0f));
	arena.step(Vector2F(1.0f, 0.0f), step_time);
	CHECK(arena.player().bounds().x == doctest::Approx(924.0f));
	CHECK(arena.player().velocity().x == doctest::Approx(240.0f));
	CHECK(arena.player().touching_trigger());
	CHECK(arena.trigger_entries() == 1);
	move(arena, Vector2F::zero, 5);
	CHECK(arena.trigger_entries() == 1);
	arena.player().set_position(Vector2F(800.0f, 320.0f));
	arena.step(Vector2F::zero, step_time);
	CHECK_FALSE(arena.player().touching_trigger());
	arena.player().set_position(Vector2F(920.0f, 320.0f));
	arena.step(Vector2F::zero, step_time);
	CHECK(arena.trigger_entries() == 2);
	arena.reset();
	CHECK(arena.trigger_entries() == 0);
	CHECK_FALSE(arena.player().touching_trigger());
	CHECK(arena.player().bounds().x == 180.0f);
}

TEST_CASE("the playable route passes decoration, stops at the wall and reaches the trigger")
{
	Arena arena;
	move(arena, Vector2F(1.0f, 0.0f), 120);
	CHECK(arena.hit_wall());
	CHECK(arena.player().bounds().right() == doctest::Approx(600.0f));
	move(arena, Vector2F(0.0f, 1.0f), 60);
	move(arena, Vector2F(1.0f, 0.0f), 100);
	move(arena, Vector2F(0.0f, -1.0f), 35);
	CHECK(arena.player().touching_trigger());
	CHECK(arena.trigger_entries() == 1);
	move(arena, Vector2F(-1.0f, -1.0f), 400);
	CHECK(arena.player().bounds().x == doctest::Approx(64.0f));
	CHECK(arena.player().bounds().y == doctest::Approx(164.0f));
	move(arena, Vector2F(1.0f, 1.0f), 500);
	CHECK(arena.player().bounds().right() == doctest::Approx(1216.0f));
	CHECK(arena.player().bounds().bottom() == doctest::Approx(640.0f));
}

TEST_CASE("a round post stops the player head on, with the same wall response")
{
	// The player's top-left corner starts at (180, 320) and it is 32 units
	// square, so its centre line is y = 336. A post centred on that line
	// meets the player's right face at the post's leftmost point.
	Arena arena;
	add_post(arena, Circle(Vector2F(300.0f, 336.0f), 20.0f));
	move(arena, Vector2F(1.0f, 0.0f), 60);
	CHECK(arena.hit_wall());
	CHECK(arena.player().bounds().right() == doctest::Approx(280.0f));
	CHECK(arena.player().bounds().y == doctest::Approx(320.0f));
}

TEST_CASE("a round post deflects a player that meets it off centre")
{
	// The post's centre is above the player's centre line, so the contact
	// normal tilts upwards and the slide carries the player down and past
	// it, where a flat wall would have stopped it dead.
	Arena arena;
	add_post(arena, Circle(Vector2F(300.0f, 310.0f), 20.0f));
	move(arena, Vector2F(1.0f, 0.0f), 60);
	CHECK(arena.hit_wall());
	CHECK(arena.player().bounds().y > 320.0f);
	CHECK(arena.player().bounds().x > 300.0f);
}

TEST_CASE("a pending entry marker starts its lifetime only after admission and retires at end_tick")
{
	Scene scene(nullptr, nullptr);
	EntryMarker* marker = scene.add(std::make_unique<EntryMarker>(Vector2F(200, 300), TextureHandle{}));
	CHECK(scene.collision_objects().empty());
	scene.update(EntryMarker::lifetime);
	CHECK_FALSE(marker->for_deletion());
	scene.end_tick();
	REQUIRE(scene.collision_objects().size() == 1);
	CHECK(scene.collision_objects()[0] == marker);

	scene.update(EntryMarker::lifetime / 2.0f);
	CHECK_FALSE(marker->for_deletion());
	scene.end_tick();
	scene.update(EntryMarker::lifetime / 2.0f);
	CHECK(marker->for_deletion());
	CHECK(scene.collision_objects().size() == 1);
	scene.end_tick();
	// The borrowed marker pointer expired. Only the scene is queried now.
	CHECK(scene.collision_objects().empty());
}

TEST_CASE("entry markers record a bounds-sweep retirement request")
{
	Scene scene(nullptr, nullptr);
	scene.set_bounds(RectangleF(0, 0, 100, 100));
	scene.add(std::make_unique<EntryMarker>(Vector2F(200, 300), TextureHandle{}));
	scene.end_tick();
	REQUIRE(scene.collision_objects().size() == 1);
	// Insertion follows retirement, so the first sweep sees it next tick.
	scene.end_tick();
	CHECK(scene.collision_objects().empty());
}

TEST_CASE("trigger entry spawns one marker, staying does not respawn it, and reset removes live markers")
{
	Arena arena;
	const std::size_t fixed_count = arena.scene().collision_objects().size();
	arena.player().set_position(Vector2F(920, 320));
	arena.step(Vector2F::zero, 0.0f);
	REQUIRE(arena.entry_marker_count() == 1);
	CHECK(arena.markers_spawned() == 1);
	CHECK(arena.scene().collision_objects().size() == fixed_count + 1);
	for (int tick = 0; tick < 4; ++tick) arena.step(Vector2F::zero, 0.125f);
	CHECK(arena.entry_marker_count() == 0);
	CHECK(arena.markers_spawned() == 1);
	CHECK(arena.scene().collision_objects().size() == fixed_count);
	arena.player().set_position(Vector2F(800, 320));
	arena.step(Vector2F::zero, 0.0f);
	arena.player().set_position(Vector2F(920, 320));
	arena.step(Vector2F::zero, 0.0f);
	REQUIRE(arena.entry_marker_count() == 1);
	CHECK(arena.markers_spawned() == 2);
	arena.reset();
	CHECK(arena.entry_marker_count() == 0);
	CHECK(arena.markers_spawned() == 0);
	CHECK(arena.scene().collision_objects().size() == fixed_count);
}

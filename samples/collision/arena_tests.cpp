#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "samples/collision/arena.h"

#include <cmath>

using namespace mattmath;
using namespace collisiondemo;

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
	move(arena, Vector2F::ZERO, 5);
	CHECK(arena.trigger_entries() == 1);
	arena.player().set_position(Vector2F(800.0f, 320.0f));
	arena.step(Vector2F::ZERO, step_time);
	CHECK_FALSE(arena.player().touching_trigger());
	arena.player().set_position(Vector2F(920.0f, 320.0f));
	arena.step(Vector2F::ZERO, step_time);
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

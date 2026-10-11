#include <doctest/doctest.h>
#include "engine/input/touch.h"
#include "engine/math/vector2i.h"

#include <cstdint>
#include <vector>

using namespace labrador;

namespace
{
	mattmath::Vector2I at(int x, int y)
	{
		return mattmath::Vector2I(x, y);
	}

	// A Touch focused for two frames, so edges are live.
	void settle(Touch& touch)
	{
		touch.set_focused(true);
		touch.poll();
		touch.poll();
	}

	std::vector<uint32_t> ids(const Touch& touch)
	{
		std::vector<uint32_t> result;
		for (const TouchPoint& point : touch.points())
		{
			result.push_back(point.id);
		}
		return result;
	}

	// The ids released between the last two polls, found the way touch.h
	// says a caller finds them: by walking the frame before.
	std::vector<uint32_t> released_ids(const Touch& touch)
	{
		std::vector<uint32_t> result;
		for (const TouchPoint& point : touch.previous_state().active())
		{
			if (touch.released(point.id))
			{
				result.push_back(point.id);
			}
		}
		return result;
	}
}

namespace TouchTests
{
	TEST_SUITE("TouchTests")
	{
		TEST_CASE("a point is pressed on the frame it appears and held after")
		{
			Touch touch;
			settle(touch);

			touch.on_down(7, at(10, 20));
			touch.poll();
			REQUIRE(touch.points().size() == 1);
			const uint32_t id = touch.points()[0].id;
			CHECK(id != 0);
			CHECK(touch.points()[0].position == at(10, 20));
			CHECK(touch.held(id));
			CHECK(touch.pressed(id));
			CHECK_FALSE(touch.released(id));

			touch.poll();
			CHECK(touch.held(id));
			CHECK_FALSE(touch.pressed(id));
		}

		TEST_CASE("a point is released on the frame after it lifts, where it was last seen")
		{
			Touch touch;
			settle(touch);
			touch.on_down(7, at(10, 20));
			touch.poll();
			const uint32_t id = touch.points()[0].id;

			touch.on_move(7, at(30, 40));
			touch.on_up(7, at(31, 41));
			touch.poll();

			CHECK(touch.points().empty());
			CHECK_FALSE(touch.held(id));
			CHECK(touch.released(id));
			CHECK_FALSE(touch.pressed(id));
			CHECK(released_ids(touch) == std::vector<uint32_t>{ id });

			// The frame before is the last one that saw it.
			CHECK(touch.previous_state().find(id)->position == at(10, 20));

			touch.poll();
			CHECK_FALSE(touch.released(id));
		}

		TEST_CASE("a tap shorter than a frame is still pressed once and released once")
		{
			Touch touch;
			settle(touch);

			touch.on_down(3, at(5, 5));
			touch.on_up(3, at(6, 6));
			touch.poll();

			REQUIRE(touch.points().size() == 1);
			const uint32_t id = touch.points()[0].id;
			CHECK(touch.pressed(id));
			CHECK(touch.points()[0].position == at(6, 6));

			touch.poll();
			CHECK(touch.points().empty());
			CHECK(touch.released(id));

			touch.poll();
			CHECK_FALSE(touch.released(id));
			CHECK_FALSE(touch.pressed(id));
		}

		TEST_CASE("several points keep their own ids, in the order they went down")
		{
			Touch touch;
			settle(touch);

			touch.on_down(100, at(1, 1));
			touch.on_down(200, at(2, 2));
			touch.poll();
			const std::vector<uint32_t> first = ids(touch);
			REQUIRE(first.size() == 2);
			CHECK(first[0] != first[1]);

			touch.on_move(200, at(20, 20));
			touch.on_move(100, at(10, 10));
			touch.on_down(300, at(3, 3));
			touch.poll();
			const std::vector<uint32_t> second = ids(touch);
			REQUIRE(second.size() == 3);
			CHECK(second[0] == first[0]);
			CHECK(second[1] == first[1]);
			CHECK(touch.motion(first[0]) == at(9, 9));
			CHECK(touch.motion(first[1]) == at(18, 18));
			CHECK(touch.pressed(second[2]));
			CHECK(touch.motion(second[2]) == at(0, 0));

			// The middle one lifts; the other two keep their places and ids.
			touch.on_up(200, at(20, 20));
			touch.poll();
			CHECK(ids(touch) == std::vector<uint32_t>{ first[0], second[2] });
			CHECK(touch.released(first[1]));
			CHECK_FALSE(touch.released(first[0]));
		}

		TEST_CASE("a platform id reused between frames is a new point")
		{
			// Windows and browsers both hand a finger's number to the next
			// finger. Keyed on theirs, this would be one point that never let
			// go; keyed on the engine's, it is a release and a press.
			Touch touch;
			settle(touch);
			touch.on_down(1, at(0, 0));
			touch.poll();
			const uint32_t before = touch.points()[0].id;

			touch.on_up(1, at(0, 0));
			touch.on_down(1, at(50, 50));
			touch.poll();
			REQUIRE(touch.points().size() == 1);
			const uint32_t after = touch.points()[0].id;

			CHECK(after != before);
			CHECK(touch.released(before));
			CHECK(touch.pressed(after));
		}

		TEST_CASE("a down for a point already down cancels the old one")
		{
			Touch touch;
			settle(touch);
			touch.on_down(4, at(0, 0));
			touch.poll();
			const uint32_t lost = touch.points()[0].id;

			touch.on_down(4, at(9, 9));
			touch.poll();
			REQUIRE(touch.points().size() == 1);
			CHECK(touch.points()[0].id != lost);
			CHECK_FALSE(touch.released(lost));
			CHECK(touch.pressed(touch.points()[0].id));
		}

		TEST_CASE("a cancelled point disappears without a release")
		{
			Touch touch;
			settle(touch);
			touch.on_down(1, at(0, 0));
			touch.on_down(2, at(5, 5));
			touch.poll();
			const std::vector<uint32_t> both = ids(touch);

			touch.on_cancel(1);
			touch.on_up(2, at(5, 5));
			touch.poll();

			CHECK(touch.points().empty());
			CHECK(touch.focused());
			CHECK_FALSE(touch.released(both[0]));
			CHECK(touch.released(both[1]));

			// And a cancelled point's later messages go nowhere.
			touch.on_move(1, at(9, 9));
			touch.on_up(1, at(9, 9));
			touch.poll();
			CHECK(touch.points().empty());
		}

		TEST_CASE("cancelling every point keeps focus and releases nothing")
		{
			Touch touch;
			settle(touch);
			touch.on_down(1, at(0, 0));
			touch.on_down(2, at(5, 5));
			touch.poll();
			const std::vector<uint32_t> both = ids(touch);

			touch.cancel_points();
			touch.poll();
			CHECK(touch.focused());
			CHECK(touch.points().empty());
			CHECK(released_ids(touch).empty());

			// A new finger after the cancel is an ordinary press.
			touch.on_down(1, at(3, 3));
			touch.poll();
			REQUIRE(touch.points().size() == 1);
			CHECK(touch.pressed(touch.points()[0].id));
			CHECK(touch.points()[0].id != both[0]);
		}

		TEST_CASE("losing focus removes every point, and no edge crosses it")
		{
			Touch touch;
			settle(touch);
			touch.on_down(1, at(0, 0));
			touch.poll();
			const uint32_t id = touch.points()[0].id;

			touch.set_focused(false);
			touch.poll();
			CHECK_FALSE(touch.focused());
			CHECK(touch.just_unfocused());
			CHECK(touch.points().empty());
			CHECK_FALSE(touch.released(id));

			// Nothing put down while away is waiting when focus returns, and
			// a finger down on the frame focus returns is held but not
			// pressed, because the frame before was not focused.
			touch.on_down(2, at(1, 1));
			touch.set_focused(true);
			touch.on_down(3, at(2, 2));
			touch.poll();
			CHECK(touch.just_focused());
			REQUIRE(touch.points().size() == 1);
			CHECK(touch.points()[0].position == at(2, 2));
			CHECK(touch.held(touch.points()[0].id));
			CHECK_FALSE(touch.pressed(touch.points()[0].id));

			// The frame after is an ordinary one.
			touch.on_down(4, at(4, 4));
			touch.poll();
			REQUIRE(touch.points().size() == 2);
			CHECK(touch.pressed(touch.points()[1].id));
		}

		TEST_CASE("focus lost and back between two polls releases nothing")
		{
			// A hidden page gets no frames, so the poll that would have seen
			// the unfocused frame never runs.
			Touch touch;
			settle(touch);
			touch.on_down(1, at(0, 0));
			touch.poll();
			const uint32_t id = touch.points()[0].id;

			touch.set_focused(false);
			touch.set_focused(true);
			touch.poll();
			CHECK(touch.focused());
			CHECK_FALSE(touch.just_unfocused());
			CHECK(touch.points().empty());
			CHECK_FALSE(touch.released(id));

			// And the frame after has no edge left over.
			touch.poll();
			CHECK_FALSE(touch.released(id));
			CHECK_FALSE(touch.pressed(id));
		}

		TEST_CASE("the first frame of the process is not a press")
		{
			Touch touch;
			touch.set_focused(true);
			touch.on_down(1, at(0, 0));
			touch.poll();
			REQUIRE(touch.points().size() == 1);
			CHECK(touch.held(touch.points()[0].id));
			CHECK_FALSE(touch.pressed(touch.points()[0].id));
		}

		TEST_CASE("past capacity a point is not seen for its whole contact")
		{
			Touch touch;
			settle(touch);
			const size_t capacity = static_cast<size_t>(TouchState::capacity);
			for (int finger = 0; finger < TouchState::capacity; ++finger)
			{
				touch.on_down(static_cast<uint32_t>(finger), at(finger, 0));
			}
			touch.on_down(99, at(99, 99));
			touch.poll();
			CHECK(touch.points().size() == capacity);
			for (const TouchPoint& point : touch.points())
			{
				CHECK(point.position != at(99, 99));
			}

			// One finger lifts and makes room; the eleventh is still not a
			// point, and its own up is dropped.
			touch.on_up(0, at(0, 0));
			touch.on_move(99, at(98, 98));
			touch.poll();
			CHECK(touch.points().size() == capacity - 1);
			touch.on_up(99, at(98, 98));
			touch.poll();
			CHECK(touch.points().size() == capacity - 1);

			// A new contact takes the room.
			touch.on_down(99, at(7, 7));
			touch.poll();
			CHECK(touch.points().size() == capacity);
			CHECK(touch.pressed(touch.points().back().id));
		}

		TEST_CASE("ids are never zero and never reused")
		{
			Touch touch;
			settle(touch);
			std::vector<uint32_t> seen;
			for (int round = 0; round < 50; ++round)
			{
				touch.on_down(1, at(0, 0));
				touch.poll();
				REQUIRE(touch.points().size() == 1);
				seen.push_back(touch.points()[0].id);
				touch.on_up(1, at(0, 0));
				touch.poll();
			}
			for (size_t i = 0; i < seen.size(); ++i)
			{
				CHECK(seen[i] != 0);
				for (size_t j = i + 1; j < seen.size(); ++j)
				{
					CHECK(seen[i] != seen[j]);
				}
			}
		}

		TEST_CASE("the free functions read two hand-written frames")
		{
			TouchState before;
			before.focused = true;
			before.count = 1;
			before.points[0] = TouchPoint{ 5, at(1, 2) };

			TouchState now;
			now.focused = true;
			now.count = 1;
			now.points[0] = TouchPoint{ 6, at(3, 4) };

			CHECK(pressed(now, before, 6));
			CHECK(released(now, before, 5));
			CHECK_FALSE(pressed(now, before, 5));
			CHECK_FALSE(pressed(now, before, 0));
			CHECK(motion(now, before, 6) == at(0, 0));

			now.cancelled_count = 1;
			now.cancelled[0] = 5;
			CHECK_FALSE(released(now, before, 5));

			before.focused = false;
			CHECK_FALSE(pressed(now, before, 6));
			CHECK(just_focused(now, before));
		}
	}
}

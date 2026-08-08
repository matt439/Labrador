#include <doctest/doctest.h>

#include "engine/ui/navigation.h"
#include "tests/ui/stub_widget.h"

using artattack::Direction;
using artattack::nearest_in_direction;
using mattmath::RectangleF;

namespace
{
	// The shape every menu page in the game hand-writes an adjacency table
	// for: a left-aligned column of three rows, 100 tall, 40 apart.
	std::vector<RectangleF> column()
	{
		return {
			RectangleF(100.0f, 0.0f, 300.0f, 100.0f),    // 0: top
			RectangleF(100.0f, 140.0f, 300.0f, 100.0f),  // 1: middle
			RectangleF(100.0f, 280.0f, 300.0f, 100.0f),  // 2: bottom
		};
	}

	// Two columns of two, for the cases a column cannot express.
	std::vector<RectangleF> grid()
	{
		return {
			RectangleF(0.0f, 0.0f, 100.0f, 100.0f),      // 0: top-left
			RectangleF(200.0f, 0.0f, 100.0f, 100.0f),    // 1: top-right
			RectangleF(0.0f, 200.0f, 100.0f, 100.0f),    // 2: bottom-left
			RectangleF(200.0f, 200.0f, 100.0f, 100.0f),  // 3: bottom-right
		};
	}
}

TEST_CASE("a column navigates down and up")
{
	const std::vector<RectangleF> rows = column();

	CHECK(nearest_in_direction(rows[0], Direction::down, rows) == 1);
	CHECK(nearest_in_direction(rows[1], Direction::down, rows) == 2);
	CHECK(nearest_in_direction(rows[2], Direction::up, rows) == 1);
	CHECK(nearest_in_direction(rows[1], Direction::up, rows) == 0);
}

TEST_CASE("a column wraps at both ends, which is what every page does by hand")
{
	const std::vector<RectangleF> rows = column();

	CHECK(nearest_in_direction(rows[2], Direction::down, rows) == 0);
	CHECK(nearest_in_direction(rows[0], Direction::up, rows) == 2);
}

TEST_CASE("wrap can be declined, and then the ends are ends")
{
	const std::vector<RectangleF> rows = column();

	CHECK(nearest_in_direction(rows[2], Direction::down, rows, false) == -1);
	CHECK(nearest_in_direction(rows[0], Direction::up, rows, false) == -1);
	CHECK(nearest_in_direction(rows[0], Direction::down, rows, false) == 1);
}

TEST_CASE("left and right in a column find nothing, and do not wrap into it")
{
	// Every row shares a centre x, so nothing is strictly left or right of
	// anything. The wrap branch must not answer here either: pressing left in
	// a vertical menu should do nothing, not jump.
	const std::vector<RectangleF> rows = column();

	CHECK(nearest_in_direction(rows[1], Direction::left, rows) == -1);
	CHECK(nearest_in_direction(rows[1], Direction::right, rows) == -1);
}

TEST_CASE("a grid goes down, not down-and-across")
{
	const std::vector<RectangleF> cells = grid();

	CHECK(nearest_in_direction(cells[0], Direction::down, cells) == 2);
	CHECK(nearest_in_direction(cells[1], Direction::down, cells) == 3);
	CHECK(nearest_in_direction(cells[0], Direction::right, cells) == 1);
	CHECK(nearest_in_direction(cells[3], Direction::left, cells) == 2);
}

TEST_CASE("the nearest wins, not the first declared")
{
	// Deliberately out of visual order: index 0 is the bottom row.
	const std::vector<RectangleF> rows = {
		RectangleF(0.0f, 400.0f, 100.0f, 100.0f),
		RectangleF(0.0f, 0.0f, 100.0f, 100.0f),
		RectangleF(0.0f, 200.0f, 100.0f, 100.0f),
	};

	CHECK(nearest_in_direction(rows[1], Direction::down, rows) == 2);
	CHECK(nearest_in_direction(rows[2], Direction::down, rows) == 0);
	CHECK(nearest_in_direction(rows[0], Direction::up, rows) == 2);
}

TEST_CASE("Direction::none never moves")
{
	const std::vector<RectangleF> rows = column();

	CHECK(nearest_in_direction(rows[0], Direction::none, rows) == -1);
}

TEST_CASE("a widget is never its own answer")
{
	const std::vector<RectangleF> only = { RectangleF(0.0f, 0.0f, 10.0f, 10.0f) };

	CHECK(nearest_in_direction(only[0], Direction::down, only) == -1);
	CHECK(nearest_in_direction(only[0], Direction::up, only) == -1);
}

TEST_CASE("degenerate boxes are not navigable")
{
	// An empty container reports a zero box. Focus landing on one is a dead
	// end the player cannot see and cannot escape.
	const std::vector<RectangleF> rows = {
		RectangleF(0.0f, 0.0f, 100.0f, 100.0f),
		RectangleF(0.0f, 150.0f, 0.0f, 0.0f),
		RectangleF(0.0f, 300.0f, 100.0f, 100.0f),
	};

	CHECK(nearest_in_direction(rows[0], Direction::down, rows) == 2);
}

TEST_CASE("the widget overload walks the same way and excludes itself by pointer")
{
	StubWidget top(100.0f, 0.0f, 300.0f, 100.0f);
	StubWidget middle(100.0f, 140.0f, 300.0f, 100.0f);
	// Same box as `top`, deliberately: by-value self-exclusion would drop
	// this one too, and only one of them is `top`.
	StubWidget twin(100.0f, 0.0f, 300.0f, 100.0f);

	const std::vector<artattack::UiWidget*> widgets = { &top, &middle, &twin };

	CHECK(nearest_in_direction(top, Direction::down, widgets) == &middle);
	CHECK(nearest_in_direction(middle, Direction::up, widgets) != nullptr);
}

TEST_CASE("null entries in the widget list are skipped, not dereferenced")
{
	StubWidget top(0.0f, 0.0f, 100.0f, 100.0f);
	StubWidget bottom(0.0f, 200.0f, 100.0f, 100.0f);

	const std::vector<artattack::UiWidget*> widgets = { &top, nullptr, &bottom };

	CHECK(nearest_in_direction(top, Direction::down, widgets) == &bottom);
}

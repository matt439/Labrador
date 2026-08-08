#include <doctest/doctest.h>

#include "engine/ui/focus.h"
#include "tests/ui/stub_widget.h"

using artattack::Direction;
using artattack::FocusGroup;
using artattack::FocusStyle;

namespace
{
	FocusStyle test_style()
	{
		FocusStyle style;
		style.focused = colour_consts::RED;
		style.unfocused = colour_consts::BLUE;
		return style;
	}
}

TEST_CASE("a group starts focused on its first widget")
{
	StubWidget play(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget options(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&play);
	group.add(&options);

	CHECK(group.focused(0) == &play);
	CHECK(group.size() == 2);
	CHECK(group.slot_count() == 1);
}

TEST_CASE("moving focus repaints both widgets")
{
	StubWidget play(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget options(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group(1, test_style());
	group.add(&play);
	group.add(&options);

	CHECK(play.colour() == colour_consts::RED);
	CHECK(options.colour() == colour_consts::BLUE);

	CHECK(group.move(0, Direction::down));

	CHECK(group.focused(0) == &options);
	CHECK(play.colour() == colour_consts::BLUE);
	CHECK(options.colour() == colour_consts::RED);
}

TEST_CASE("move reports whether focus actually moved")
{
	StubWidget only(0.0f, 0.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&only);

	// Nowhere to go, wrap or not: a caller keying the cursor sound off this
	// stays silent instead of clicking at a wall.
	CHECK_FALSE(group.move(0, Direction::down));
	CHECK_FALSE(group.move(0, Direction::none));
	CHECK(group.focused(0) == &only);
}

TEST_CASE("activate runs the focused widget's action and nothing else")
{
	StubWidget play(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget options(0.0f, 100.0f, 100.0f, 50.0f);

	int plays = 0;
	int opens = 0;

	FocusGroup group;
	group.add(&play, [&plays] { plays++; });
	group.add(&options, [&opens] { opens++; });

	CHECK(group.activate(0));
	CHECK(plays == 1);
	CHECK(opens == 0);

	group.move(0, Direction::down);
	CHECK(group.activate(0));
	CHECK(plays == 1);
	CHECK(opens == 1);
}

TEST_CASE("a widget with no action is focusable and activating it is a no-op")
{
	// The rows that change a value on left/right and mean nothing on A.
	StubWidget resolution(0.0f, 0.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&resolution);

	CHECK_FALSE(group.activate(0));
}

TEST_CASE("set_focused rejects a widget the group does not hold")
{
	StubWidget inside(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget outside(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&inside);

	CHECK_FALSE(group.set_focused(0, &outside));
	CHECK_FALSE(group.set_focused(0, nullptr));
	CHECK(group.focused(0) == &inside);
}

TEST_CASE("two slots hold independent cursors over one widget set")
{
	StubWidget a(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget b(0.0f, 100.0f, 100.0f, 50.0f);
	StubWidget c(0.0f, 200.0f, 100.0f, 50.0f);

	FocusGroup group(2, test_style());
	group.add(&a);
	group.add(&b);
	group.add(&c);

	CHECK(group.move(0, Direction::down));
	CHECK(group.focused(0) == &b);
	// Slot 1 has not moved. This is the case that does not exist anywhere in
	// the game today and that PHILOSOPHY promises for split-screen.
	CHECK(group.focused(1) == &a);

	CHECK(group.move(1, Direction::down));
	CHECK(group.move(1, Direction::down));
	CHECK(group.focused(1) == &c);
	CHECK(group.focused(0) == &b);
}

TEST_CASE("a widget stays painted focused while any slot is still on it")
{
	StubWidget shared(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget other(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group(2, test_style());
	group.add(&shared);
	group.add(&other);

	// Both slots start on `shared`. Slot 0 leaves; slot 1 is still there.
	CHECK(group.move(0, Direction::down));

	CHECK(group.focused(0) == &other);
	CHECK(group.focused(1) == &shared);
	// Incremental repainting - unpaint what I left, paint what I entered -
	// would have blanked `shared` here even though slot 1 is on it.
	CHECK(shared.colour() == colour_consts::RED);
	CHECK(other.colour() == colour_consts::RED);
}

TEST_CASE("changing the style repaints from the current focus")
{
	StubWidget a(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget b(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&a);
	group.add(&b);
	group.move(0, Direction::down);

	group.set_style(test_style());

	CHECK(a.colour() == colour_consts::BLUE);
	CHECK(b.colour() == colour_consts::RED);
}

TEST_CASE("a slot index outside the group throws rather than clamping")
{
	StubWidget only(0.0f, 0.0f, 100.0f, 50.0f);

	FocusGroup group(2);
	group.add(&only);

	CHECK_THROWS_AS(group.focused(2), std::out_of_range);
	CHECK_THROWS_AS(group.focused(-1), std::out_of_range);
}

TEST_CASE("a group must have at least one slot, and widgets must be real")
{
	CHECK_THROWS_AS(FocusGroup(0), std::invalid_argument);

	FocusGroup group;
	CHECK_THROWS_AS(group.add(nullptr), std::invalid_argument);
}

TEST_CASE("clear empties the group and focus reports nothing")
{
	StubWidget a(0.0f, 0.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&a);
	group.clear();

	CHECK(group.size() == 0);
	CHECK(group.focused(0) == nullptr);
	CHECK_FALSE(group.activate(0));
	CHECK_FALSE(group.move(0, Direction::down));
}

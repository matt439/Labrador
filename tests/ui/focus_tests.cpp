#include <doctest/doctest.h>

#include "engine/ui/focus.h"
#include "tests/ui/stub_widget.h"

using labrador::Direction;
using labrador::FocusGroup;
using labrador::Activation;
using labrador::FocusStyle;

namespace
{
	FocusStyle test_style()
	{
		FocusStyle style;
		style.focused = labrador::Colour::red;
		style.unfocused = labrador::Colour::blue;
		style.disabled = labrador::Colour::green;
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

	CHECK(play.colour() == labrador::Colour::red);
	CHECK(options.colour() == labrador::Colour::blue);

	CHECK(group.move(0, Direction::down));

	CHECK(group.focused(0) == &options);
	CHECK(play.colour() == labrador::Colour::blue);
	CHECK(options.colour() == labrador::Colour::red);
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

	CHECK(group.activate(0) == Activation::ran);
	CHECK(plays == 1);
	CHECK(opens == 0);

	group.move(0, Direction::down);
	CHECK(group.activate(0) == Activation::ran);
	CHECK(plays == 1);
	CHECK(opens == 1);
}

TEST_CASE("a widget with no action is focusable and activating it is a no-op")
{
	// The rows that change a value on left/right and mean nothing on A.
	StubWidget resolution(0.0f, 0.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&resolution);

	CHECK(group.activate(0) == Activation::none);
}

TEST_CASE("a disabled row is registered, painted apart, and jumped over")
{
	// Mode Select: four modes advertised, one built. The other three used to
	// be left unregistered, which works and costs the beep - there is no way
	// to be on Deathmatch, so there is no press to answer and no way to say
	// why it is unavailable.
	StubWidget standard(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget deathmatch(0.0f, 100.0f, 100.0f, 50.0f);
	StubWidget capture(0.0f, 200.0f, 100.0f, 50.0f);

	FocusGroup group(1, test_style());
	group.add(&standard);
	group.add(&deathmatch);
	group.add(&capture);

	CHECK(group.set_enabled(&deathmatch, false));
	CHECK_FALSE(group.enabled(&deathmatch));

	// Registered, so it is still in the group and still drawn - in a colour
	// that says what it is.
	CHECK(group.size() == 3);
	CHECK(deathmatch.colour() == labrador::Colour::green);

	// Jumped over rather than stopped at: one press from the top row reaches
	// the third, not the second.
	CHECK(group.move(0, Direction::down));
	CHECK(group.focused(0) == &capture);
}

TEST_CASE("activating a disabled row is refused rather than ignored")
{
	StubWidget standard(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget deathmatch(0.0f, 100.0f, 100.0f, 50.0f);

	int standards = 0;
	int deathmatches = 0;

	FocusGroup group(1, test_style());
	group.add(&standard, [&standards] { standards++; });
	// An action bound and unreachable, which is the ordinary case: the row is
	// waiting on something, not empty.
	group.add(&deathmatch, [&deathmatches] { deathmatches++; });
	group.set_enabled(&deathmatch, false);

	CHECK(group.set_focused(0, &deathmatch));

	// The whole of the ask. `none` would have been indistinguishable from a
	// row that simply has nothing bound, and the page needs to tell those
	// apart to say why - which it does by asking who is focused.
	CHECK(group.activate(0) == Activation::refused);
	CHECK(deathmatches == 0);
	CHECK(group.focused(0) == &deathmatch);

	CHECK(group.set_focused(0, &standard));
	CHECK(group.activate(0) == Activation::ran);
	CHECK(standards == 1);
}

TEST_CASE("a row switched off under the cursor keeps the cursor and the exit")
{
	// Player Count, which cannot use Mode Select's trick: its rows become
	// available while the page is open, because a pad can be plugged in
	// between two frames, and a page that re-registered its group every frame
	// to track that would reset every cursor every frame with it.
	StubWidget two(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget three(0.0f, 100.0f, 100.0f, 50.0f);
	StubWidget four(0.0f, 200.0f, 100.0f, 50.0f);

	FocusGroup group(1, test_style());
	group.add(&two);
	group.add(&three);
	group.add(&four);

	group.move(0, Direction::down);
	group.move(0, Direction::down);
	REQUIRE(group.focused(0) == &four);

	// The fourth pad is unplugged with the player sitting on that row.
	group.set_enabled(&four, false);

	// The cursor stays. Yanking it elsewhere mid-press is worse than a row
	// that answers and says why.
	CHECK(group.focused(0) == &four);

	// Disabled outranks focused in the paint, and it has to: painting it
	// focused would be the screen offering a row at the moment it refuses to
	// be picked.
	CHECK(four.colour() == labrador::Colour::green);
	CHECK(group.activate(0) == Activation::refused);

	// And it is not a dead end. `from` is passed to the walk as itself rather
	// than as a candidate, so the cursor walks off a row it is not allowed to
	// be on.
	CHECK(group.move(0, Direction::up));
	CHECK(group.focused(0) == &three);
	CHECK(three.colour() == labrador::Colour::red);

	// Plugged back in between two frames: the row is live again, in every
	// sense, with no cursor disturbed.
	group.set_enabled(&four, true);
	CHECK(four.colour() == labrador::Colour::blue);
	CHECK(group.move(0, Direction::down));
	CHECK(group.focused(0) == &four);
	CHECK(group.activate(0) == Activation::none);
}

TEST_CASE("enablement answers false for a widget the group does not hold")
{
	StubWidget inside(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget outside(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group;
	group.add(&inside);

	CHECK_FALSE(group.set_enabled(&outside, false));
	CHECK_FALSE(group.set_enabled(nullptr, false));
	CHECK_FALSE(group.enabled(&outside));
	CHECK_FALSE(group.enabled(nullptr));

	// The one it does hold is enabled, because a row a page went to the
	// trouble of registering is one it means to offer.
	CHECK(group.enabled(&inside));
}

TEST_CASE("a group with nothing left enabled stops moving and refuses")
{
	StubWidget a(0.0f, 0.0f, 100.0f, 50.0f);
	StubWidget b(0.0f, 100.0f, 100.0f, 50.0f);

	FocusGroup group(1, test_style());
	group.add(&a, [] {});
	group.add(&b, [] {});
	group.set_enabled(&a, false);
	group.set_enabled(&b, false);

	// No candidates, so nowhere to go - and the cursor is still on something,
	// so there is still a press to answer. A page in this state has a sentence
	// to say and the engine leaves it able to say it.
	CHECK_FALSE(group.move(0, Direction::down));
	CHECK(group.focused(0) == &a);
	CHECK(group.activate(0) == Activation::refused);
	CHECK(a.colour() == labrador::Colour::green);
	CHECK(b.colour() == labrador::Colour::green);
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
	CHECK(shared.colour() == labrador::Colour::red);
	CHECK(other.colour() == labrador::Colour::red);
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

	CHECK(a.colour() == labrador::Colour::blue);
	CHECK(b.colour() == labrador::Colour::red);
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
	CHECK(group.activate(0) == Activation::none);
	CHECK_FALSE(group.move(0, Direction::down));
}

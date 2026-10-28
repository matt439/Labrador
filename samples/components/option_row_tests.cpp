#include <doctest/doctest.h>

#include "samples/components/option_row.h"
#include "samples/components/test_content.h"

#include "engine/input/direction.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/ui/focus.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using components::OptionRow;
using components::TestContent;
using labrador::Colour;
using labrador::Direction;
using labrador::FocusGroup;
using labrador::RectangleF;
using labrador::Vector2F;

namespace
{
	std::vector<std::wstring> difficulties()
	{
		return { L"Easy", L"Normal", L"Hard" };
	}
}

TEST_CASE("an option row starts on the choice it was given")
{
	TestContent content;
	const OptionRow row("difficulty", L"Difficulty", difficulties(), 1,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources);

	CHECK(row.index() == 1);
	CHECK(row.choice() == L"Normal");
	CHECK(row.value().text() == L"Normal");
	CHECK(row.choice_count() == 3);
}

TEST_CASE("right and left step through the choices, and report each change")
{
	TestContent content;
	std::vector<size_t> heard;
	OptionRow row("difficulty", L"Difficulty", difficulties(), 0,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources,
		[&heard](size_t index) { heard.push_back(index); });

	CHECK(row.adjust(Direction::right));
	CHECK(row.choice() == L"Normal");
	CHECK(row.adjust(Direction::right));
	CHECK(row.choice() == L"Hard");
	CHECK(row.adjust(Direction::left));
	CHECK(row.choice() == L"Normal");
	CHECK(row.value().text() == L"Normal");

	CHECK(heard == std::vector<size_t>{ 1, 2, 1 });
}

TEST_CASE("a wrapping row goes round at both ends")
{
	TestContent content;
	OptionRow row("window", L"Window", { L"Windowed", L"Full screen" }, 1,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources);

	CHECK(row.adjust(Direction::right));
	CHECK(row.index() == 0);
	CHECK(row.value().text() == L"Windowed");
	CHECK(row.adjust(Direction::left));
	CHECK(row.index() == 1);
	CHECK(row.value().text() == L"Full screen");
}

TEST_CASE("a two-choice window row follows external fullscreen snapshots without callbacks")
{
	TestContent content;
	std::vector<bool> requests;
	OptionRow row("window", L"Window", { L"Windowed", L"Full screen" }, 0,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources,
		[&requests](size_t index) { requests.push_back(index == 1); });

	REQUIRE(row.choice_count() == 2);
	// The page's synchronization recipe, using external snapshots rather
	// than a window or Application. These changes issue no new request.
	for (const bool fullscreen : { true, false, true, false })
	{
		row.set_index(fullscreen ? 1 : 0);
		CHECK(row.index() == (fullscreen ? 1u : 0u));
		CHECK(row.value().text() == (fullscreen ? L"Full screen" : L"Windowed"));
		CHECK(requests.empty());
	}

	CHECK(row.adjust(Direction::right));
	CHECK(row.index() == 1);
	CHECK(requests == std::vector<bool>{ true });

	// A later refusal or external exit corrects the requested choice.
	row.set_index(0);
	CHECK(row.index() == 0);
	CHECK(row.value().text() == L"Windowed");
	row.set_index(0);
	CHECK(requests == std::vector<bool>{ true });
}

TEST_CASE("a row that does not wrap stops at its ends, says so and stays quiet")
{
	TestContent content;
	int calls = 0;
	OptionRow row("difficulty", L"Difficulty", difficulties(), 2,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources,
		[&calls](size_t) { ++calls; }, false);

	CHECK_FALSE(row.adjust(Direction::right));
	CHECK(row.choice() == L"Hard");
	CHECK(row.value().text() == L"Hard");

	row.set_index(0);
	CHECK(row.value().text() == L"Easy");
	CHECK_FALSE(row.adjust(Direction::left));
	CHECK(row.choice() == L"Easy");

	// set_index is the page speaking, so it is not reported back to it.
	CHECK(calls == 0);
}

TEST_CASE("up, down and none are not the row's")
{
	TestContent content;
	OptionRow row("difficulty", L"Difficulty", difficulties(), 1,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources);

	CHECK_FALSE(row.adjust(Direction::up));
	CHECK_FALSE(row.adjust(Direction::down));
	CHECK_FALSE(row.adjust(Direction::none));
	CHECK(row.index() == 1);
}

TEST_CASE("a row with nothing to choose, or a start outside its list, throws")
{
	TestContent content;

	CHECK_THROWS_AS(OptionRow("empty", L"Empty", {}, 0,
		Vector2F::zero, 300.0f, "font", &content.resources),
		std::invalid_argument);
	CHECK_THROWS_AS(OptionRow("past", L"Past", difficulties(), 3,
		Vector2F::zero, 300.0f, "font", &content.resources),
		std::invalid_argument);

	OptionRow row("difficulty", L"Difficulty", difficulties(), 0,
		Vector2F::zero, 300.0f, "font", &content.resources);
	CHECK_THROWS_AS(row.set_index(3), std::invalid_argument);
	CHECK(row.index() == 0);
}

TEST_CASE("the row's box runs from the label to the end of the value")
{
	TestContent content;
	OptionRow row("difficulty", L"Difficulty", difficulties(), 1,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources);

	// "Normal" is six ten-unit cells starting at 300, on a twenty-unit line.
	// The navigation walk reads this box, so the row is one target.
	const RectangleF bounds = row.bounds();
	CHECK(bounds.x == doctest::Approx(100.0f));
	CHECK(bounds.y == doctest::Approx(50.0f));
	CHECK(bounds.width == doctest::Approx(260.0f));
	CHECK(bounds.height == doctest::Approx(20.0f));

	// And it follows the value when that changes length: "Hard" is shorter.
	row.adjust(Direction::right);
	CHECK(row.bounds().width == doctest::Approx(240.0f));
}

TEST_CASE("focus paints the label and leaves the value its own colour")
{
	TestContent content;
	OptionRow first("difficulty", L"Difficulty", difficulties(), 0,
		Vector2F(100.0f, 50.0f), 300.0f, "font", &content.resources);
	OptionRow second("window", L"Window", { L"Windowed", L"Full screen" }, 0,
		Vector2F(100.0f, 100.0f), 300.0f, "font", &content.resources);

	const Colour value_before = second.value().colour();

	FocusGroup focus(1, { Colour::red, Colour::blue, Colour::dark_gray });
	focus.add(&first);
	focus.add(&second);

	// Down lands on the second row as a whole, and only its label turns.
	CHECK(focus.move(0, Direction::down));
	CHECK(focus.focused(0) == &second);

	CHECK(second.label().colour() == Colour::red);
	CHECK(second.value().colour() == value_before);
	CHECK(first.label().colour() == Colour::blue);
	CHECK(first.value().colour() == value_before);
}

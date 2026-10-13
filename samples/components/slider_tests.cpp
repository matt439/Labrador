#include <doctest/doctest.h>

#include "samples/components/slider.h"
#include "samples/components/test_content.h"

#include "engine/input/direction.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/ui/focus.h"

#include <climits>
#include <stdexcept>
#include <vector>

using components::Slider;
using components::TestContent;
using labrador::Colour;
using labrador::Direction;
using labrador::FocusGroup;
using labrador::TextureHandle;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	// A volume, 0 to 100 in tens, over a track two hundred units long, so
	// every step is twenty units of fill.
	const RectangleF track(300.0f, 54.0f, 200.0f, 12.0f);

	Slider volume(TestContent& content, int value,
		Slider::Changed on_change = nullptr)
	{
		return Slider("music", L"Music", 0, 100, 10, value,
			Vector2F(100.0f, 50.0f), track, "font", TextureHandle(),
			&content.resources, std::move(on_change));
	}
}

TEST_CASE("a slider starts on its value, with the fill that far along")
{
	TestContent content;
	const Slider slider = volume(content, 70);

	CHECK(slider.value() == 70);
	CHECK(slider.number().text() == L"70");

	const RectangleF fill = slider.fill().bounds();
	CHECK(fill.x == doctest::Approx(300.0f));
	CHECK(fill.y == doctest::Approx(54.0f));
	CHECK(fill.width == doctest::Approx(140.0f));
	CHECK(fill.height == doctest::Approx(12.0f));
}

TEST_CASE("right and left move by a step, and report each change")
{
	TestContent content;
	std::vector<int> heard;
	Slider slider = volume(content, 50,
		[&heard](int value) { heard.push_back(value); });

	CHECK(slider.adjust(Direction::right));
	CHECK(slider.adjust(Direction::left));
	CHECK(slider.adjust(Direction::left));

	CHECK(slider.value() == 40);
	CHECK(slider.number().text() == L"40");
	CHECK(slider.fill().bounds().width == doctest::Approx(80.0f));
	CHECK(heard == std::vector<int>{ 60, 50, 40 });
}

TEST_CASE("a slider clamps at both ends and never wraps")
{
	TestContent content;
	int calls = 0;
	Slider slider = volume(content, 100, [&calls](int) { ++calls; });

	CHECK_FALSE(slider.adjust(Direction::right));
	CHECK(slider.value() == 100);
	CHECK(slider.fill().bounds().width == doctest::Approx(200.0f));

	slider.set_value(0);
	CHECK_FALSE(slider.adjust(Direction::left));
	CHECK(slider.value() == 0);
	CHECK(slider.fill().bounds().width == doctest::Approx(0.0f));

	// set_value is the page speaking, so it is not reported back to it.
	CHECK(calls == 0);
}

TEST_CASE("a step that overshoots the end stops on the end")
{
	TestContent content;
	Slider slider("sensitivity", L"Sensitivity", 1, 10, 4, 9,
		Vector2F(100.0f, 50.0f), track, "font", TextureHandle(),
		&content.resources);

	CHECK(slider.adjust(Direction::right));
	CHECK(slider.value() == 10);
	CHECK(slider.adjust(Direction::left));
	CHECK(slider.value() == 6);
}

TEST_CASE("a range that does not start at zero fills from its minimum")
{
	TestContent content;
	Slider slider("speed", L"Speed", 10, 20, 4, 15,
		Vector2F(100.0f, 50.0f), track, "font", TextureHandle(),
		&content.resources);

	// Five of ten is half the track.
	CHECK(slider.fill().bounds().width == doctest::Approx(100.0f));

	// And a step that overshoots the minimum stops on it, with no fill.
	CHECK(slider.adjust(Direction::left));
	CHECK(slider.value() == 11);
	CHECK(slider.adjust(Direction::left));
	CHECK(slider.value() == 10);
	CHECK(slider.number().text() == L"10");
	CHECK(slider.fill().bounds().width == doctest::Approx(0.0f));

	CHECK(slider.adjust(Direction::right));
	CHECK(slider.value() == 14);
	CHECK(slider.fill().bounds().width == doctest::Approx(80.0f));
}

TEST_CASE("a range as wide as an int steps and fills without overflowing")
{
	TestContent content;
	Slider slider("wide", L"Wide", INT_MIN, INT_MAX, 1000, INT_MAX - 5,
		Vector2F(100.0f, 50.0f), track, "font", TextureHandle(),
		&content.resources);

	CHECK(slider.adjust(Direction::right));
	CHECK(slider.value() == INT_MAX);
	CHECK(slider.fill().bounds().width == doctest::Approx(200.0f));

	slider.set_value(INT_MIN + 5);
	CHECK(slider.adjust(Direction::left));
	CHECK(slider.value() == INT_MIN);
	CHECK(slider.fill().bounds().width == doctest::Approx(0.0f));

	slider.set_value(0);
	CHECK(slider.fill().bounds().width == doctest::Approx(100.0f));
}

TEST_CASE("up, down and none are not the slider's")
{
	TestContent content;
	Slider slider = volume(content, 50);

	CHECK_FALSE(slider.adjust(Direction::up));
	CHECK_FALSE(slider.adjust(Direction::down));
	CHECK_FALSE(slider.adjust(Direction::none));
	CHECK(slider.value() == 50);
}

TEST_CASE("a range that is not one, or a value outside it, throws")
{
	TestContent content;
	const auto make = [&content](int minimum, int maximum, int step, int value)
	{
		return Slider("bad", L"Bad", minimum, maximum, step, value,
			Vector2F::ZERO, track, "font", TextureHandle(), &content.resources);
	};

	CHECK_THROWS_AS(make(10, 10, 1, 10), std::invalid_argument);
	CHECK_THROWS_AS(make(10, 0, 1, 5), std::invalid_argument);
	CHECK_THROWS_AS(make(0, 10, 0, 5), std::invalid_argument);
	CHECK_THROWS_AS(make(0, 10, 1, 11), std::invalid_argument);
	CHECK_THROWS_AS(make(0, 10, 1, -1), std::invalid_argument);

	Slider slider = volume(content, 50);
	CHECK_THROWS_AS(slider.set_value(101), std::invalid_argument);
	CHECK_THROWS_AS(slider.set_value(-1), std::invalid_argument);
	CHECK(slider.value() == 50);
	CHECK(slider.number().text() == L"50");
}

TEST_CASE("the slider's box runs from the label past the number")
{
	TestContent content;
	const Slider slider = volume(content, 100);

	// The number "100" starts sixteen units after the track's end, at 516,
	// and is three ten-unit cells wide.
	const RectangleF bounds = slider.bounds();
	CHECK(bounds.x == doctest::Approx(100.0f));
	CHECK(bounds.y == doctest::Approx(50.0f));
	CHECK(bounds.width == doctest::Approx(446.0f));
	CHECK(bounds.height == doctest::Approx(20.0f));
}

TEST_CASE("focus lights the label and the fill, and leaves the track and the number")
{
	TestContent content;
	Slider music = volume(content, 50);
	Slider effects("effects", L"Effects", 0, 100, 10, 50,
		Vector2F(100.0f, 100.0f), RectangleF(300.0f, 104.0f, 200.0f, 12.0f),
		"font", TextureHandle(), &content.resources);

	FocusGroup focus(1, { Colour::red, Colour::blue, Colour::dark_gray });
	focus.add(&music);
	focus.add(&effects);

	CHECK(music.label().colour() == Colour::red);
	CHECK(music.fill().colour() == Colour::red);
	CHECK(effects.fill().colour() == Colour::blue);

	const Colour track_before = music.track().colour();
	const Colour number_before = music.number().colour();

	CHECK(focus.move(0, Direction::down));
	CHECK(effects.fill().colour() == Colour::red);
	CHECK(music.fill().colour() == Colour::blue);
	CHECK(music.track().colour() == track_before);
	CHECK(music.number().colour() == number_before);
}

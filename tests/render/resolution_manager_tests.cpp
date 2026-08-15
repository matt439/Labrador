#include <doctest/doctest.h>

#include "engine/render/resolution_manager.h"
#include "engine/render/screen_resolution.h"
#include "engine/math/vector2i.h"

#include <stdexcept>

using labrador::ResolutionManager;
using labrador::ScreenResolution;
using mattmath::Vector2I;

TEST_CASE("the default size is the default preset's size")
{
	// Two members, one decision. They cannot be initialised separately and
	// drift, because the constructor runs the setter.
	const ResolutionManager manager;

	CHECK(manager.resolution() == ScreenResolution::s_1280_720);
	CHECK(manager.resolution_ivec() == Vector2I(1280, 720));
	CHECK(manager.resolution_vec().x == doctest::Approx(1280.0f));
	CHECK(manager.resolution_vec().y == doctest::Approx(720.0f));
}

TEST_CASE("a preset sets the size and the label together")
{
	ResolutionManager manager;

	manager.set_resolution(ScreenResolution::s_2560_1440);

	CHECK(manager.resolution() == ScreenResolution::s_2560_1440);
	CHECK(manager.resolution_ivec() == Vector2I(2560, 1440));
	CHECK(manager.resolution_string() == "2560x1440");
}

TEST_CASE("an exact size outside the presets is stored as given")
{
	// THE DEFECT, IN ONE ASSERTION. A window can be any size at all, and the
	// enum has four values - so every size outside those four used to be
	// answered with 1280x720 and every layout above here was computed for a
	// window that was not on the screen.
	ResolutionManager manager;

	manager.set_resolution_exactly(Vector2I(1600, 900));

	CHECK(manager.resolution_ivec() == Vector2I(1600, 900));
	CHECK(manager.resolution_vec().x == doctest::Approx(1600.0f));

	// A size with no name in the enum keeps the last named one, rather than
	// having a wrong name invented for it.
	CHECK(manager.resolution() == ScreenResolution::s_1280_720);
}

TEST_CASE("a size that is not 16:9 survives, because a window need not be")
{
	// The one thing this hands a client that it did not have before: the closed
	// enum made a non-16:9 size unreachable, and a dragged window edge reaches
	// it immediately.
	ResolutionManager manager;

	manager.set_resolution_exactly(Vector2I(1000, 1000));

	CHECK(manager.resolution_ivec() == Vector2I(1000, 1000));
}

TEST_CASE("an exact size that is a preset adopts its label")
{
	ResolutionManager manager;

	manager.set_resolution_exactly(Vector2I(1920, 1080));

	CHECK(manager.resolution_ivec() == Vector2I(1920, 1080));
	CHECK(manager.resolution() == ScreenResolution::s_1920_1080);
}

TEST_CASE("an exact size one pixel off a preset does not adopt its label")
{
	// The round-trip test earning its keep. convert_ivec_to_resolution answers
	// s_1280_720 for anything it does not recognise, so a naive "ask it what
	// this is" would have called 1919x1080 720p.
	ResolutionManager manager;
	manager.set_resolution(ScreenResolution::s_2560_1440);

	manager.set_resolution_exactly(Vector2I(1919, 1080));

	CHECK(manager.resolution_ivec() == Vector2I(1919, 1080));
	CHECK(manager.resolution() == ScreenResolution::s_2560_1440);
}

TEST_CASE("the coercing overload still coerces, and that is why there are two")
{
	// Unchanged behaviour, pinned so nobody unifies the two setters by accident.
	// This overload has always meant "pick the preset nearest this" and answers
	// 720p for anything it does not recognise; pushing a window size through it
	// is what would have made the fix a no-op.
	ResolutionManager manager;

	manager.set_resolution(Vector2I(1600, 900));

	CHECK(manager.resolution() == ScreenResolution::s_1280_720);
	CHECK(manager.resolution_ivec() == Vector2I(1280, 720));
}

TEST_CASE("a non-positive extent is refused rather than divided by")
{
	ResolutionManager manager;

	CHECK_THROWS_AS(manager.set_resolution_exactly(Vector2I(0, 720)),
		std::invalid_argument);
	CHECK_THROWS_AS(manager.set_resolution_exactly(Vector2I(1280, 0)),
		std::invalid_argument);
	CHECK_THROWS_AS(manager.set_resolution_exactly(Vector2I(-1280, -720)),
		std::invalid_argument);

	// And it threw before it wrote.
	CHECK(manager.resolution_ivec() == Vector2I(1280, 720));
}

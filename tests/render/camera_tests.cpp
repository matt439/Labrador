#include <doctest/doctest.h>

#include "engine/render/camera.h"
#include "engine/render/viewport.h"

#include <stdexcept>

using artattack::Camera;
using artattack::Viewport;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	// What `viewport` actually shows of the world, under `camera`. This is the
	// inverse of Camera::calculate_view_rectangle, and it is what the caller
	// of frame() cares about: not the numbers in the camera, but the region
	// they put on screen.
	RectangleF shown(const Camera& camera, const Viewport& viewport)
	{
		return RectangleF(camera.translation,
			Vector2F(viewport.width / camera.scale,
				viewport.height / camera.scale));
	}

	bool contains(const RectangleF& outer, const RectangleF& inner)
	{
		return inner.x >= outer.x - 0.01f &&
			inner.y >= outer.y - 0.01f &&
			inner.x + inner.width <= outer.x + outer.width + 0.01f &&
			inner.y + inner.height <= outer.y + outer.height + 0.01f;
	}
}

TEST_SUITE("Camera::frame")
{
	TEST_CASE("a matching aspect is shown exactly, with no slack")
	{
		const Viewport screen(RectangleF(0.0f, 0.0f, 1920.0f, 1080.0f));
		const RectangleF world(0.0f, 4000.0f, 3840.0f, 2160.0f);

		const Camera camera = Camera::frame(world, screen);

		CHECK(camera.scale == doctest::Approx(0.5f));
		CHECK(camera.translation.x == doctest::Approx(0.0f));
		CHECK(camera.translation.y == doctest::Approx(4000.0f));

		const RectangleF visible = shown(camera, screen);
		CHECK(visible.width == doctest::Approx(3840.0f));
		CHECK(visible.height == doctest::Approx(2160.0f));
	}

	TEST_CASE("the height is read, not derived from the width")
	{
		// This is the defect the whole function was rewritten for. The old
		// form set scale = world.width / view.width and never looked at
		// world.height, so these two - same width, one twice as tall - came
		// out as the same camera.
		const Viewport screen(RectangleF(0.0f, 0.0f, 1920.0f, 1080.0f));

		const Camera wide = Camera::frame(RectangleF(0.0f, 0.0f, 6000.0f,
			1000.0f), screen);
		const Camera tall = Camera::frame(RectangleF(0.0f, 0.0f, 6000.0f,
			6000.0f), screen);

		CHECK(wide.scale != doctest::Approx(tall.scale));

		// The tall one is the one the height has to constrain: 6000 wide into
		// 1920 would be 0.32, but 6000 tall into 1080 is 0.18, and the smaller
		// is the one that fits.
		CHECK(tall.scale == doctest::Approx(1080.0f / 6000.0f));
		CHECK(wide.scale == doctest::Approx(1920.0f / 6000.0f));
	}

	TEST_CASE("everything asked for is visible, whichever axis is tight")
	{
		const Viewport screen(RectangleF(0.0f, 0.0f, 1920.0f, 1080.0f));

		const RectangleF letterboxed(-100.0f, 3800.0f, 6200.0f, 3300.0f);
		const RectangleF pillarboxed(1000.0f, 0.0f, 500.0f, 4000.0f);

		CHECK(contains(shown(Camera::frame(letterboxed, screen), screen),
			letterboxed));
		CHECK(contains(shown(Camera::frame(pillarboxed, screen), screen),
			pillarboxed));
	}

	TEST_CASE("the surplus is split evenly, so the request stays centred")
	{
		const Viewport screen(RectangleF(0.0f, 0.0f, 1000.0f, 1000.0f));

		// Half as tall as it is wide, into a square: the vertical surplus is
		// the whole difference, and half of it goes above.
		const RectangleF world(0.0f, 0.0f, 400.0f, 200.0f);
		const RectangleF visible = shown(Camera::frame(world, screen), screen);

		CHECK(visible.width == doctest::Approx(400.0f));
		CHECK(visible.height == doctest::Approx(400.0f));
		CHECK(visible.x == doctest::Approx(0.0f));
		CHECK(visible.y == doctest::Approx(-100.0f));

		const float top_slack = world.y - visible.y;
		const float bottom_slack =
			(visible.y + visible.height) - (world.y + world.height);
		CHECK(top_slack == doctest::Approx(bottom_slack));
	}

	TEST_CASE("a degenerate extent throws instead of returning an infinity")
	{
		const Viewport screen(RectangleF(0.0f, 0.0f, 1920.0f, 1080.0f));
		const Viewport nothing(RectangleF(0.0f, 0.0f, 0.0f, 0.0f));

		CHECK_THROWS_AS(Camera::frame(RectangleF(0.0f, 0.0f, 0.0f, 100.0f),
			screen), std::invalid_argument);
		CHECK_THROWS_AS(Camera::frame(RectangleF(0.0f, 0.0f, 100.0f, 0.0f),
			screen), std::invalid_argument);
		CHECK_THROWS_AS(Camera::frame(RectangleF(0.0f, 0.0f, 100.0f, 100.0f),
			nothing), std::invalid_argument);
	}

	TEST_CASE("framing round-trips through calculate_view_rectangle")
	{
		const Viewport screen(RectangleF(0.0f, 0.0f, 1920.0f, 1080.0f));
		const RectangleF world(-100.0f, 4300.0f, 6200.0f, 2800.0f);

		const Camera camera = Camera::frame(world, screen);
		const RectangleF on_screen = camera.calculate_view_rectangle(world);

		// It lands inside the pane, and on the tight axis it fills it.
		CHECK(on_screen.x >= -0.01f);
		CHECK(on_screen.y >= -0.01f);
		CHECK(on_screen.width == doctest::Approx(1920.0f));
		CHECK(on_screen.width <= screen.width + 0.01f);
		CHECK(on_screen.height <= screen.height + 0.01f);
	}
}

#include <doctest/doctest.h>

#include "engine/render/camera.h"
#include "engine/render/viewport.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <stdexcept>

using artattack::Camera;
using artattack::Viewport;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	// What `viewport` actually shows of the world, under `camera` - the
	// inverse of Camera::calculate_view_rectangle, and what the caller of
	// frame() cares about: not the numbers in the camera, but the region they
	// put on screen.
	//
	// This was the only correct inverse in the tree, and it was private to
	// this file while three production sites wrote their own and multiplied
	// where they should divide. It is Camera::visible_rectangle now, and this
	// forwards so the cases below keep reading as they did.
	RectangleF shown(const Camera& camera, const Viewport& viewport)
	{
		return camera.visible_rectangle(viewport);
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

	TEST_CASE("visible_rectangle is the inverse of calculate_view_rectangle")
	{
		const Viewport screen(RectangleF(0.0f, 0.0f, 1920.0f, 1080.0f));

		SUBCASE("a rectangle taken through both transforms comes back")
		{
			const Camera camera(Vector2F(500.0f, 300.0f), 2.0f);
			const RectangleF visible = camera.visible_rectangle(screen);

			// Push the visible region forward and it should be exactly the
			// pane, at the origin.
			const RectangleF on_screen =
				camera.calculate_view_rectangle(visible);

			CHECK(on_screen.x == doctest::Approx(0.0f));
			CHECK(on_screen.y == doctest::Approx(0.0f));
			CHECK(on_screen.width == doctest::Approx(screen.width));
			CHECK(on_screen.height == doctest::Approx(screen.height));
		}

		SUBCASE("zooming out shows MORE of the world, not less")
		{
			// The direction the multiply got backwards. This is the case
			// Camera::frame produces for any world larger than its pane, and
			// the one a cull is built on.
			const Camera zoomed_out(Vector2F::ZERO, 0.5f);
			const Camera zoomed_in(Vector2F::ZERO, 2.0f);

			CHECK(zoomed_out.visible_rectangle(screen).width >
				zoomed_in.visible_rectangle(screen).width);

			CHECK(zoomed_out.visible_rectangle(screen).width ==
				doctest::Approx(screen.width * 2.0f));
		}

		SUBCASE("a framed camera sees exactly what it was asked to frame")
		{
			// 6000 world units into 1080 pixels is a scale of 0.18, where
			// multiplying reported a visible region 31 times too small in each
			// axis - so a scene culling against it discarded almost everything
			// that was on screen.
			const RectangleF world(0.0f, 0.0f, 6000.0f, 6000.0f);
			const RectangleF visible =
				Camera::frame(world, screen).visible_rectangle(screen);

			CHECK(visible.height == doctest::Approx(6000.0f));
			CHECK(visible.width >= 6000.0f);
			CHECK(contains(visible, world));
		}

		SUBCASE("a zero scale is refused rather than divided by")
		{
			const Camera blind(Vector2F::ZERO, 0.0f);

			CHECK_THROWS_AS(blind.visible_rectangle(screen),
				std::invalid_argument);
		}
	}
}

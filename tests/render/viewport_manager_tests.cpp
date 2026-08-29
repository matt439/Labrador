#include <doctest/doctest.h>
#include <vector>

#include "engine/render/resolution_manager.h"
#include "engine/render/screen_layout.h"
#include "engine/render/viewport_manager.h"
#include "engine/math/rectanglef.h"
#include "engine/math/vector2i.h"

using labrador::ResolutionManager;
using labrador::ScreenLayout;
using labrador::ViewportManager;
using labrador::Viewport;

namespace
{
	// Constructible at all only because ViewportManager stopped holding a
	// DeviceResources it never read.
	ResolutionManager default_resolution()
	{
		ResolutionManager manager;
		manager.set_resolution(labrador::ScreenResolution::s_1280_720);
		return manager;
	}

	bool same(const Viewport& viewport,
		float x, float y, float width, float height)
	{
		return viewport.x == doctest::Approx(x)
			&& viewport.y == doctest::Approx(y)
			&& viewport.width == doctest::Approx(width)
			&& viewport.height == doctest::Approx(height);
	}

	bool is_fullscreen(const Viewport& viewport)
	{
		return same(viewport, 0.0f, 0.0f, 1280.0f, 720.0f);
	}
}

TEST_CASE("one player gets the whole screen")
{
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);
	manager.set_layout(ScreenLayout::one_player);

	CHECK(manager.all_viewports().size() == 1);
	CHECK(is_fullscreen(manager.player_viewport(0)));
}

TEST_CASE("two players split horizontally")
{
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);
	manager.set_layout(ScreenLayout::two_player);

	const std::vector<Viewport> viewports = manager.all_viewports();
	REQUIRE(viewports.size() == 2);
	CHECK(same(viewports[0], 0.0f, 0.0f, 1280.0f, 360.0f));
	CHECK(same(viewports[1], 0.0f, 360.0f, 1280.0f, 360.0f));
}

TEST_CASE("four players get the four quadrants")
{
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);
	manager.set_layout(ScreenLayout::four_player);

	const std::vector<Viewport> viewports = manager.all_viewports();
	REQUIRE(viewports.size() == 4);
	CHECK(same(viewports[0], 0.0f, 0.0f, 640.0f, 360.0f));
	CHECK(same(viewports[1], 640.0f, 0.0f, 640.0f, 360.0f));
	CHECK(same(viewports[2], 0.0f, 360.0f, 640.0f, 360.0f));
	CHECK(same(viewports[3], 640.0f, 360.0f, 640.0f, 360.0f));
}

TEST_CASE("three players get four quadrants, and the fourth is not the whole screen")
{
	// The defect: all_viewports() asked for index 4, which no layout has. It
	// fell through calculate_viewport's switch to the fullscreen fallback, so
	// the fourth pane covered the other three and every menu drew a fourth
	// full-screen pass over itself.
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);
	manager.set_layout(ScreenLayout::three_player);

	const std::vector<Viewport> viewports = manager.all_viewports();
	REQUIRE(viewports.size() == 4);

	for (const Viewport& viewport : viewports)
	{
		CHECK_FALSE(is_fullscreen(viewport));
		CHECK(viewport.width == doctest::Approx(640.0f));
		CHECK(viewport.height == doctest::Approx(360.0f));
	}

	CHECK(same(viewports[0], 0.0f, 0.0f, 640.0f, 360.0f));       // top left
	CHECK(same(viewports[1], 640.0f, 360.0f, 640.0f, 360.0f));   // bottom right
	CHECK(same(viewports[2], 0.0f, 360.0f, 640.0f, 360.0f));     // bottom left
	CHECK(same(viewports[3], 640.0f, 0.0f, 640.0f, 360.0f));     // the empty one
}

TEST_CASE("all_viewports tile the screen exactly, with no overlap")
{
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);

	for (const ScreenLayout layout : { ScreenLayout::one_player,
		ScreenLayout::two_player, ScreenLayout::three_player,
		ScreenLayout::four_player })
	{
		manager.set_layout(layout);
		const std::vector<Viewport> viewports = manager.all_viewports();

		float area = 0.0f;
		for (const Viewport& viewport : viewports)
		{
			area += viewport.width * viewport.height;
		}

		CAPTURE(static_cast<int>(layout));
		CHECK(area == doctest::Approx(1280.0f * 720.0f));
	}
}

TEST_CASE("an out-of-range index does not answer from the next layout down")
{
	// An inner switch that falls through to the next layout's cases has
	// two_player(9) answered by the three-player table.
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);

	manager.set_layout(ScreenLayout::two_player);
	CHECK(is_fullscreen(manager.player_viewport(9)));
	CHECK(is_fullscreen(manager.player_viewport(-1)));

	manager.set_layout(ScreenLayout::three_player);
	CHECK(is_fullscreen(manager.player_viewport(9)));

	manager.set_layout(ScreenLayout::four_player);
	CHECK(is_fullscreen(manager.player_viewport(9)));
}

TEST_CASE("the divider count matches the layout")
{
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);

	manager.set_layout(ScreenLayout::one_player);
	CHECK(manager.viewport_dividers().empty());

	manager.set_layout(ScreenLayout::two_player);
	CHECK(manager.viewport_dividers().size() == 1);

	manager.set_layout(ScreenLayout::three_player);
	CHECK(manager.viewport_dividers().size() == 2);

	manager.set_layout(ScreenLayout::four_player);
	CHECK(manager.viewport_dividers().size() == 2);
}

TEST_CASE("the layout follows a window size that is not a preset")
{
	// THE CONSUMER HALF OF THE RESOLUTION FIX, and the reason that fix was
	// filed as high. This class derives every viewport and divider from
	// resolution_vec(), and it has no device pointer to read a real back
	// buffer from instead - deliberately, so that it can be constructed in a
	// test like this one. So the whole of its correctness rests on the
	// resolution manager being told when the window changed size.
	//
	// Before that happened, a full-screen toggle to 1440p left this laying
	// every pane out for 1280x720 in the top-left corner of a 2560x1440 back
	// buffer, with the rest cleared black. 1600x900 is deliberately not one of
	// the four presets: routed through the coercing setter this test would see
	// 1280x720 and pass for the wrong reason.
	ResolutionManager resolution = default_resolution();
	ViewportManager manager(&resolution);
	manager.set_layout(ScreenLayout::two_player);

	resolution.set_resolution_exactly(mattmath::Vector2I(1600, 900));

	CHECK(same(manager.fullscreen_viewport(), 0.0f, 0.0f, 1600.0f, 900.0f));

	// Top and bottom halves of the new size, not of the old one.
	CHECK(same(manager.player_viewport(0), 0.0f, 0.0f, 1600.0f, 450.0f));
	CHECK(same(manager.player_viewport(1), 0.0f, 450.0f, 1600.0f, 450.0f));

	// And the divider is still centred on the screen it is actually dividing.
	const std::vector<mattmath::RectangleF> dividers =
		manager.viewport_dividers();
	REQUIRE(dividers.size() == 1);
	CHECK(dividers[0].width == doctest::Approx(1600.0f));
	CHECK(dividers[0].center().y == doctest::Approx(450.0f));
}

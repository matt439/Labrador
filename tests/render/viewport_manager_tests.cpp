#include <doctest/doctest.h>
#include <vector>

#include "engine/render/resolution_manager.h"
#include "engine/render/screen_layout.h"
#include "engine/render/viewport_manager.h"

using artattack::ResolutionManager;
using artattack::ScreenLayout;
using artattack::ViewportManager;
using artattack::Viewport;

namespace
{
	// Constructible at all only because ViewportManager stopped holding a
	// DeviceResources it never read.
	ResolutionManager default_resolution()
	{
		ResolutionManager manager;
		manager.set_resolution(artattack::ScreenResolution::s_1280_720);
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
	// Each layout's inner switch used to fall through to the next one's cases,
	// so two_player(9) was answered by the three-player table.
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

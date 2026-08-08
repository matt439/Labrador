#include <doctest/doctest.h>

#include "engine/render/camera_tools.h"

using artattack::BorderThickness;
using artattack::CameraTools;
using artattack::Camera;
using mattmath::RectangleF;
using mattmath::Vector2F;

namespace
{
	// The four viewport sizes the shipped game actually produces at its
	// default resolution. 1280x720 is ApplicationOptions' default,
	// ResolutionManager's default, and what an unparseable save file coerces
	// to, so these are not edge cases - three of the four are the common path.
	const Vector2F ONE_PLAYER = { 1280.0f, 720.0f };
	const Vector2F TWO_PLAYER = { 1280.0f, 360.0f };
	const Vector2F THREE_OR_FOUR_PLAYER = { 640.0f, 360.0f };

	// Far larger than any camera moves in these tests, so the bounds clamp
	// never fires and the border model is what is under test.
	const RectangleF UNBOUNDED = { -100000.0f, -100000.0f, 200000.0f, 200000.0f };
}

TEST_CASE("opposing scroll borders never sum past the viewport")
{
	// The one line that pins the worst of it. Before the fix this was 500
	// against a 360-tall pane: the floor was clamped in first and the ceiling
	// was then never consulted, so a minimum sat above its own maximum.
	for (const Vector2F& viewport :
		{ ONE_PLAYER, TWO_PLAYER, THREE_OR_FOUR_PLAYER })
	{
		const BorderThickness border =
			CameraTools::calculate_camera_scroll_border(viewport);

		CAPTURE(viewport.x);
		CAPTURE(viewport.y);
		CHECK(border.left + border.right <= viewport.x);
		CHECK(border.top + border.bottom <= viewport.y);
		CHECK(border.left >= 0.0f);
		CHECK(border.top >= 0.0f);
		CHECK(border.right >= 0.0f);
		CHECK(border.bottom >= 0.0f);
	}
}

TEST_CASE("no border exceeds half its own axis")
{
	for (const Vector2F& viewport :
		{ ONE_PLAYER, TWO_PLAYER, THREE_OR_FOUR_PLAYER })
	{
		const BorderThickness border =
			CameraTools::calculate_camera_scroll_border(viewport);

		CAPTURE(viewport.x);
		CAPTURE(viewport.y);
		CHECK(border.left <= viewport.x / 2.0f);
		CHECK(border.right <= viewport.x / 2.0f);
		CHECK(border.top <= viewport.y / 2.0f);
		CHECK(border.bottom <= viewport.y / 2.0f);
	}
}

TEST_CASE("a viewport large enough for the floors gets the ratio, not the floor")
{
	// Fullscreen at 1280x720: 40% of 1280 is 512, above the 400 floor and
	// below the 640 ceiling, so nothing clamps and the ratio comes through.
	const BorderThickness border =
		CameraTools::calculate_camera_scroll_border(ONE_PLAYER);

	CHECK(border.left == doctest::Approx(512.0f));
	CHECK(border.right == doctest::Approx(512.0f));
	CHECK(border.top == doctest::Approx(250.0f));    // 144 raised to the floor
	CHECK(border.bottom == doctest::Approx(288.0f)); // 40% of 720
}

TEST_CASE("a motionless player does not make the camera judder")
{
	// The bug this exists for. With top_edge below bottom_edge, a stationary
	// player satisfies one branch, gets pushed past the other, and satisfies
	// that one next frame - 140 px of vertical swing at 60 Hz, forever, in
	// two-player at the default resolution.
	const CameraTools tools;

	for (const Vector2F& viewport :
		{ ONE_PLAYER, TWO_PLAYER, THREE_OR_FOUR_PLAYER })
	{
		const Vector2F player_center = { viewport.x / 2.0f, viewport.y / 2.0f };

		Camera camera;
		camera.translation = Vector2F::ZERO;

		// Long enough for any real settling to finish.
		for (int frame = 0; frame < 32; frame++)
		{
			camera = tools.calculate_camera(
				player_center, viewport, camera, UNBOUNDED);
		}

		const Camera settled = camera;
		const Camera next = tools.calculate_camera(
			player_center, viewport, settled, UNBOUNDED);

		CAPTURE(viewport.x);
		CAPTURE(viewport.y);
		CHECK(next.translation.x == doctest::Approx(settled.translation.x));
		CHECK(next.translation.y == doctest::Approx(settled.translation.y));
	}
}

TEST_CASE("the camera follows a player who leaves the dead zone")
{
	// The other half: the fix must not have frozen the camera.
	const CameraTools tools;

	Camera camera;
	camera.translation = Vector2F::ZERO;

	const Camera moved = tools.calculate_camera(
		{ 5000.0f, 300.0f }, TWO_PLAYER, camera, UNBOUNDED);

	CHECK(moved.translation.x > camera.translation.x);
}

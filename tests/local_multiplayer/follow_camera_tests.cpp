#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "samples/local_multiplayer/follow_camera.h"

#include <array>
#include <limits>
#include <stdexcept>

using namespace labrador;
using multiplayer::FollowCamera;

namespace
{
	struct FollowInput
	{
		Vector2F target = Vector2F(500.0f, 300.0f);
		Vector2F viewport = Vector2F(200.0f, 100.0f);
		RectangleF dead_zone = RectangleF(60.0f, 30.0f, 80.0f, 40.0f);
		RectangleF world = RectangleF(0.0f, 0.0f, 2000.0f, 2000.0f);
	};

	void update_camera(FollowCamera& follow, const FollowInput& input)
	{
		follow.update(input.target, input.viewport, input.dead_zone, input.world);
	}

	void check_origin(const FollowCamera& follow, float x, float y)
	{
		const Camera camera = follow.camera();
		CHECK(camera.translation.x == doctest::Approx(x));
		CHECK(camera.translation.y == doctest::Approx(y));
		CHECK(camera.scale == 1.0f);
	}

	void check_refused_without_mutation(FollowCamera& follow, const FollowInput& invalid)
	{
		const Camera before = follow.camera();
		CHECK_THROWS_AS(update_camera(follow, invalid), std::invalid_argument);
		CHECK(follow.camera() == before);
		// Refusal must preserve the remembered pane size as well as the camera.
		// A later unchanged update must not act as a resize or a first update.
		update_camera(follow, FollowInput{});
		CHECK(follow.camera() == before);
	}
}

TEST_CASE("a follow camera starts at identity and first centers its target")
{
	FollowCamera follow;
	const FollowCamera& query = follow;
	CHECK(query.camera() == Camera::default_camera);
	update_camera(follow, FollowInput{});
	check_origin(query, 400.0f, 250.0f);
	CHECK(query.camera().calculate_view_position(Vector2F(500.0f, 300.0f)) ==
		Vector2F(100.0f, 50.0f));
}

TEST_CASE("inclusive dead-zone edges and corners leave the origin stationary")
{
	FollowCamera follow;
	FollowInput input;
	update_camera(follow, input);
	const std::array<Vector2F, 8> targets = {
		Vector2F(460.0f, 300.0f), Vector2F(540.0f, 300.0f),
		Vector2F(500.0f, 280.0f), Vector2F(500.0f, 320.0f),
		Vector2F(460.0f, 280.0f), Vector2F(540.0f, 280.0f),
		Vector2F(460.0f, 320.0f), Vector2F(540.0f, 320.0f)
	};
	for (const Vector2F& target : targets)
	{
		CAPTURE(target.x);
		CAPTURE(target.y);
		input.target = target;
		update_camera(follow, input);
		check_origin(follow, 400.0f, 250.0f);
	}
}

TEST_CASE("one-pixel escapes pan only the axes and distance needed")
{
	struct PanCase
	{
		Vector2F target;
		Vector2F origin;
	};
	const std::array<PanCase, 8> cases = {
		PanCase{ Vector2F(459.0f, 300.0f), Vector2F(399.0f, 250.0f) },
		PanCase{ Vector2F(541.0f, 300.0f), Vector2F(401.0f, 250.0f) },
		PanCase{ Vector2F(500.0f, 279.0f), Vector2F(400.0f, 249.0f) },
		PanCase{ Vector2F(500.0f, 321.0f), Vector2F(400.0f, 251.0f) },
		PanCase{ Vector2F(459.0f, 279.0f), Vector2F(399.0f, 249.0f) },
		PanCase{ Vector2F(541.0f, 279.0f), Vector2F(401.0f, 249.0f) },
		PanCase{ Vector2F(459.0f, 321.0f), Vector2F(399.0f, 251.0f) },
		PanCase{ Vector2F(541.0f, 321.0f), Vector2F(401.0f, 251.0f) }
	};
	for (const PanCase& test : cases)
	{
		CAPTURE(test.target.x);
		CAPTURE(test.target.y);
		FollowCamera follow;
		FollowInput input;
		update_camera(follow, input);
		input.target = test.target;
		update_camera(follow, input);
		check_origin(follow, test.origin.x, test.origin.y);
	}
}

TEST_CASE("a followed target that stops or returns inside the zone causes no drift")
{
	FollowCamera follow;
	FollowInput input;
	update_camera(follow, input);
	input.target = Vector2F(600.0f, 350.0f);
	update_camera(follow, input);
	check_origin(follow, 460.0f, 280.0f);
	for (int update = 0; update < 60; ++update)
	{
		update_camera(follow, input);
	}
	check_origin(follow, 460.0f, 280.0f);
	input.target = Vector2F(550.0f, 330.0f);
	update_camera(follow, input);
	check_origin(follow, 460.0f, 280.0f);
}

TEST_CASE("an asymmetric first-update zone adjusts the centered view minimally")
{
	FollowCamera follow;
	FollowInput input;
	SUBCASE("zone at top left")
	{
		input.dead_zone = RectangleF(0.0f, 0.0f, 20.0f, 10.0f);
		update_camera(follow, input);
		check_origin(follow, 480.0f, 290.0f);
	}
	SUBCASE("zone at bottom right")
	{
		input.dead_zone = RectangleF(180.0f, 80.0f, 20.0f, 20.0f);
		update_camera(follow, input);
		check_origin(follow, 320.0f, 220.0f);
	}
	SUBCASE("zero-area zone follows an exact point")
	{
		input.dead_zone = RectangleF(100.0f, 50.0f, 0.0f, 0.0f);
		update_camera(follow, input);
		input.target = Vector2F(501.0f, 301.0f);
		update_camera(follow, input);
		check_origin(follow, 401.0f, 251.0f);
	}
	SUBCASE("zero-area zone can lie at the pane's inclusive far corner")
	{
		input.dead_zone = RectangleF(200.0f, 100.0f, 0.0f, 0.0f);
		update_camera(follow, input);
		check_origin(follow, 300.0f, 200.0f);
	}
}

TEST_CASE("offset world edges override dead-zone following on every side")
{
	struct ClampCase
	{
		Vector2F target;
		Vector2F origin;
	};
	const std::array<ClampCase, 6> cases = {
		ClampCase{ Vector2F(-100.0f, 350.0f), Vector2F(100.0f, 300.0f) },
		ClampCase{ Vector2F(900.0f, 350.0f), Vector2F(500.0f, 300.0f) },
		ClampCase{ Vector2F(400.0f, 100.0f), Vector2F(300.0f, 200.0f) },
		ClampCase{ Vector2F(400.0f, 900.0f), Vector2F(300.0f, 400.0f) },
		ClampCase{ Vector2F(100.0f, 200.0f), Vector2F(100.0f, 200.0f) },
		ClampCase{ Vector2F(700.0f, 500.0f), Vector2F(500.0f, 400.0f) }
	};
	for (const ClampCase& test : cases)
	{
		FollowCamera follow;
		FollowInput input;
		input.world = RectangleF(100.0f, 200.0f, 600.0f, 300.0f);
		input.target = Vector2F(400.0f, 350.0f);
		update_camera(follow, input);
		input.target = test.target;
		CAPTURE(test.target.x);
		CAPTURE(test.target.y);
		update_camera(follow, input);
		check_origin(follow, test.origin.x, test.origin.y);
	}
}

TEST_CASE("small-world axes are centered independently including zero extents")
{
	FollowCamera follow;
	FollowInput input;
	SUBCASE("only the width is smaller than the pane")
	{
		input.world = RectangleF(100.0f, 200.0f, 100.0f, 400.0f);
		input.target = Vector2F(120.0f, 400.0f);
		update_camera(follow, input);
		check_origin(follow, 50.0f, 350.0f);
		input.target = Vector2F(-1000.0f, 900.0f);
		update_camera(follow, input);
		check_origin(follow, 50.0f, 500.0f);
	}
	SUBCASE("only the height is smaller than the pane")
	{
		input.world = RectangleF(100.0f, 200.0f, 600.0f, 40.0f);
		input.target = Vector2F(400.0f, 220.0f);
		update_camera(follow, input);
		check_origin(follow, 300.0f, 170.0f);
		input.target = Vector2F(999.0f, -1000.0f);
		update_camera(follow, input);
		check_origin(follow, 500.0f, 170.0f);
	}
	SUBCASE("world exactly the size of the pane")
	{
		input.world = RectangleF(100.0f, 200.0f, 200.0f, 100.0f);
		update_camera(follow, input);
		check_origin(follow, 100.0f, 200.0f);
	}
	SUBCASE("world is one point")
	{
		input.world = RectangleF(100.0f, 200.0f, 0.0f, 0.0f);
		update_camera(follow, input);
		check_origin(follow, 0.0f, 150.0f);
	}
	SUBCASE("world has zero width and ordinary height")
	{
		input.world = RectangleF(100.0f, 200.0f, 0.0f, 400.0f);
		input.target = Vector2F(100.0f, 350.0f);
		update_camera(follow, input);
		check_origin(follow, 0.0f, 300.0f);
	}
	SUBCASE("world has ordinary width and zero height")
	{
		input.world = RectangleF(100.0f, 200.0f, 600.0f, 0.0f);
		update_camera(follow, input);
		check_origin(follow, 400.0f, 150.0f);
	}
}

TEST_CASE("resizing preserves the previous world view center rather than the target")
{
	FollowCamera follow;
	FollowInput input;
	update_camera(follow, input);
	input.target = Vector2F(600.0f, 350.0f);
	update_camera(follow, input);
	check_origin(follow, 460.0f, 280.0f);
	// The old center is (560, 330); the target is deliberately off center.
	input.viewport = Vector2F(300.0f, 200.0f);
	input.dead_zone = RectangleF(0.0f, 0.0f, 300.0f, 200.0f);
	update_camera(follow, input);
	check_origin(follow, 410.0f, 230.0f);
	input.viewport = Vector2F(100.0f, 50.0f);
	input.dead_zone = RectangleF(0.0f, 0.0f, 100.0f, 50.0f);
	update_camera(follow, input);
	check_origin(follow, 510.0f, 305.0f);
}

TEST_CASE("resizing still applies the new dead zone and world clamp after preserving center")
{
	FollowCamera follow;
	FollowInput input;
	update_camera(follow, input);
	SUBCASE("target outside the new zone is nudged to its nearest edge")
	{
		input.viewport = Vector2F(300.0f, 200.0f);
		input.dead_zone = RectangleF(100.0f, 50.0f, 100.0f, 100.0f);
		input.target = Vector2F(700.0f, 300.0f);
		update_camera(follow, input);
		check_origin(follow, 500.0f, 200.0f);
	}
	SUBCASE("a pane larger than the new world centers the world instead")
	{
		input.viewport = Vector2F(900.0f, 600.0f);
		input.dead_zone = RectangleF(0.0f, 0.0f, 900.0f, 600.0f);
		input.world = RectangleF(100.0f, 200.0f, 600.0f, 300.0f);
		update_camera(follow, input);
		check_origin(follow, -50.0f, 50.0f);
	}
}

TEST_CASE("follow instances and const camera values have independent state")
{
	FollowCamera first;
	FollowCamera second;
	FollowInput input;
	update_camera(first, input);
	input.target = Vector2F(800.0f, 500.0f);
	update_camera(second, input);
	input.target = Vector2F(600.0f, 350.0f);
	update_camera(first, input);
	const FollowCamera& first_query = first;
	const FollowCamera& second_query = second;
	Camera local_copy = first_query.camera();
	local_copy.translation = Vector2F(-100.0f, -200.0f);
	CHECK(local_copy.translation == Vector2F(-100.0f, -200.0f));
	check_origin(first_query, 460.0f, 280.0f);
	check_origin(second_query, 700.0f, 450.0f);
}

TEST_CASE("every nonfinite input field is refused without changing follow state")
{
	FollowCamera follow;
	update_camera(follow, FollowInput{});
	FollowInput invalid;
	struct Field
	{
		const char* name;
		float* value;
	};
	const std::array<Field, 12> fields = {
		Field{ "target.x", &invalid.target.x }, Field{ "target.y", &invalid.target.y },
		Field{ "viewport.x", &invalid.viewport.x }, Field{ "viewport.y", &invalid.viewport.y },
		Field{ "dead_zone.x", &invalid.dead_zone.x }, Field{ "dead_zone.y", &invalid.dead_zone.y },
		Field{ "dead_zone.width", &invalid.dead_zone.width },
		Field{ "dead_zone.height", &invalid.dead_zone.height },
		Field{ "world.x", &invalid.world.x }, Field{ "world.y", &invalid.world.y },
		Field{ "world.width", &invalid.world.width }, Field{ "world.height", &invalid.world.height }
	};
	const std::array<float, 3> values = {
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN()
	};
	for (const Field& field : fields)
	{
		const float original = *field.value;
		for (const float value : values)
		{
			CAPTURE(field.name);
			CAPTURE(value);
			*field.value = value;
			check_refused_without_mutation(follow, invalid);
		}
		*field.value = original;
	}
}

TEST_CASE("invalid extents and out-of-pane dead zones are refused atomically")
{
	FollowCamera follow;
	update_camera(follow, FollowInput{});
	for (const Vector2F& viewport : std::array<Vector2F, 4>{
		Vector2F(0.0f, 100.0f), Vector2F(200.0f, 0.0f),
		Vector2F(-1.0f, 100.0f), Vector2F(200.0f, -1.0f) })
	{
		FollowInput invalid;
		invalid.viewport = viewport;
		check_refused_without_mutation(follow, invalid);
	}
	for (const RectangleF& zone : std::array<RectangleF, 8>{
		RectangleF(-1.0f, 30.0f, 80.0f, 40.0f), RectangleF(60.0f, -1.0f, 80.0f, 40.0f),
		RectangleF(60.0f, 30.0f, -1.0f, 40.0f), RectangleF(60.0f, 30.0f, 80.0f, -1.0f),
		RectangleF(121.0f, 30.0f, 80.0f, 40.0f), RectangleF(60.0f, 61.0f, 80.0f, 40.0f),
		RectangleF(201.0f, 30.0f, 0.0f, 0.0f), RectangleF(0.0f, 101.0f, 0.0f, 0.0f) })
	{
		FollowInput invalid;
		invalid.dead_zone = zone;
		check_refused_without_mutation(follow, invalid);
	}
	for (const RectangleF& world : std::array<RectangleF, 2>{
		RectangleF(0.0f, 0.0f, -1.0f, 1000.0f), RectangleF(0.0f, 0.0f, 1000.0f, -1.0f) })
	{
		FollowInput invalid;
		invalid.world = world;
		check_refused_without_mutation(follow, invalid);
	}
}

TEST_CASE("unrepresentable geometry is refused before committing follow state")
{
	FollowCamera follow;
	update_camera(follow, FollowInput{});
	FollowInput invalid;
	const float largest = std::numeric_limits<float>::max();
	SUBCASE("a finite world rectangle has an unrepresentable endpoint")
	{
		invalid.world = RectangleF(largest, 0.0f, largest, 1000.0f);
	}
	SUBCASE("a centered point world requires an unrepresentable camera translation")
	{
		invalid.target = Vector2F::zero;
		invalid.viewport = Vector2F(largest, 100.0f);
		invalid.dead_zone = RectangleF::zero;
		invalid.world = RectangleF(-largest, 0.0f, 0.0f, 0.0f);
	}
	check_refused_without_mutation(follow, invalid);
}

TEST_CASE("a refused first update leaves the camera uninitialized for its first valid target")
{
	FollowCamera follow;
	FollowInput invalid;
	invalid.world.height = -1.0f;
	CHECK_THROWS_AS(update_camera(follow, invalid), std::invalid_argument);
	CHECK(follow.camera() == Camera::default_camera);
	update_camera(follow, FollowInput{});
	check_origin(follow, 400.0f, 250.0f);
}

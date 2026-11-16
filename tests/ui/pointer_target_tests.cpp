#include <doctest/doctest.h>

#include "engine/ui/pointer_target.h"

#include <array>
#include <limits>
#include <optional>
#include <vector>

using namespace labrador;

TEST_CASE("a hit is the first enabled target, inside its left and top edges")
{
	const std::vector<PointerTarget> targets = {
		{ RectangleF(0.0f, 0.0f, 100.0f, 50.0f), true },
		{ RectangleF(0.0f, 50.0f, 100.0f, 50.0f), false },
		{ RectangleF(0.0f, 50.0f, 200.0f, 50.0f), true },
	};

	CHECK(hit_pointer_target(targets, Vector2F(0.0f, 0.0f)) == std::optional<size_t>(0));
	CHECK(hit_pointer_target(targets, Vector2F(99.5f, 49.5f)) == std::optional<size_t>(0));

	// The shared edge belongs to the row below, and the disabled row there
	// neither answers nor hides the enabled one under it.
	CHECK(hit_pointer_target(targets, Vector2F(10.0f, 50.0f)) == std::optional<size_t>(2));
	CHECK(hit_pointer_target(targets, Vector2F(150.0f, 60.0f)) == std::optional<size_t>(2));

	CHECK_FALSE(hit_pointer_target(targets, Vector2F(100.0f, 10.0f)).has_value());
	CHECK_FALSE(hit_pointer_target(targets, Vector2F(-0.5f, 10.0f)).has_value());
	CHECK_FALSE(hit_pointer_target(targets, Vector2F(10.0f, 100.0f)).has_value());
	CHECK_FALSE(hit_pointer_target({}, Vector2F(10.0f, 10.0f)).has_value());
}

TEST_CASE("enabled overlaps keep authored order and original indices")
{
	std::array<PointerTarget, 3> targets = { {
		{ RectangleF(-20.0f, -10.0f, 40.0f, 20.0f), false },
		{ RectangleF(-10.0f, -5.0f, 20.0f, 10.0f), true },
		{ RectangleF(-30.0f, -20.0f, 60.0f, 40.0f), true }
	} };
	CHECK(hit_pointer_target(targets, Vector2F::zero) == std::optional<size_t>(1));
	targets[0].enabled = true;
	CHECK(hit_pointer_target(targets, Vector2F::zero) == std::optional<size_t>(0));
	CHECK(hit_pointer_target(targets, Vector2F(-20.0f, -10.0f)) == std::optional<size_t>(0));
	CHECK(hit_pointer_target(targets, Vector2F(20.0f, 0.0f)) == std::optional<size_t>(2));
	CHECK(hit_pointer_target(targets, Vector2F(0.0f, 10.0f)) == std::optional<size_t>(2));
	targets[0].enabled = false;
	targets[1].enabled = false;
	targets[2].enabled = false;
	CHECK_FALSE(hit_pointer_target(targets, Vector2F::zero));
}

TEST_CASE("targets and their endpoints may lie entirely in negative page coordinates")
{
	const std::array<PointerTarget, 1> targets = { {
		{ RectangleF(-20.0f, -20.0f, 5.0f, 5.0f), true }
	} };
	CHECK(hit_pointer_target(targets, Vector2F(-20.0f, -20.0f)) == std::optional<size_t>(0));
	CHECK(hit_pointer_target(targets, Vector2F(-18.0f, -18.0f)) == std::optional<size_t>(0));
	CHECK_FALSE(hit_pointer_target(targets, Vector2F(-15.0f, -18.0f)));
}

TEST_CASE("invalid target geometry misses and cannot hide a valid target")
{
	const float infinity = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float maximum = (std::numeric_limits<float>::max)();
	const std::array<RectangleF, 12> invalid = {
		RectangleF(0.0f, 0.0f, 0.0f, 10.0f),
		RectangleF(0.0f, 0.0f, 10.0f, 0.0f),
		RectangleF(0.0f, 0.0f, -10.0f, 10.0f),
		RectangleF(0.0f, 0.0f, 10.0f, -10.0f),
		RectangleF(nan, 0.0f, 10.0f, 10.0f),
		RectangleF(0.0f, nan, 10.0f, 10.0f),
		RectangleF(0.0f, 0.0f, nan, 10.0f),
		RectangleF(0.0f, 0.0f, 10.0f, nan),
		RectangleF(0.0f, 0.0f, infinity, 10.0f),
		RectangleF(0.0f, 0.0f, 10.0f, infinity),
		RectangleF(maximum, 0.0f, maximum, 10.0f),
		RectangleF(0.0f, maximum, 10.0f, maximum)
	};
	for (const RectangleF& rectangle : invalid)
	{
		const std::array<PointerTarget, 2> targets = { {
			{ rectangle, true }, { RectangleF(0.0f, 0.0f, 10.0f, 10.0f), true }
		} };
		CHECK_FALSE(hit_pointer_target(std::span<const PointerTarget>(targets).first(1), Vector2F(1.0f, 1.0f)));
		CHECK(hit_pointer_target(targets, Vector2F(1.0f, 1.0f)) == std::optional<size_t>(1));
	}
}

TEST_CASE("nonfinite pointer positions never choose a target")
{
	const std::array<PointerTarget, 1> targets = { {
		{ RectangleF(-10.0f, -10.0f, 20.0f, 20.0f), true }
	} };
	const float infinity = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	for (const Vector2F& point : {
		Vector2F(infinity, 0.0f), Vector2F(-infinity, 0.0f), Vector2F(nan, 0.0f),
		Vector2F(0.0f, infinity), Vector2F(0.0f, -infinity), Vector2F(0.0f, nan) })
	{
		CHECK_FALSE(hit_pointer_target(targets, point));
	}
}

TEST_CASE("overflowed endpoints cannot make an unbounded target")
{
	const float maximum = (std::numeric_limits<float>::max)();
	const std::array<PointerTarget, 2> horizontal = { {
		{ RectangleF(maximum * 0.5f, 0.0f, maximum, 10.0f), true },
		{ RectangleF(0.0f, 0.0f, maximum, 10.0f), true }
	} };
	CHECK(hit_pointer_target(horizontal, Vector2F(maximum * 0.75f, 1.0f)) ==
		std::optional<size_t>(1));
	const std::array<PointerTarget, 2> vertical = { {
		{ RectangleF(0.0f, maximum * 0.5f, 10.0f, maximum), true },
		{ RectangleF(0.0f, 0.0f, 10.0f, maximum), true }
	} };
	CHECK(hit_pointer_target(vertical, Vector2F(1.0f, maximum * 0.75f)) ==
		std::optional<size_t>(1));
}

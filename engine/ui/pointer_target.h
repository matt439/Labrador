#pragma once

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"

#include <cstddef>
#include <optional>
#include <span>

namespace labrador
{
	// A page-authored pointer target, in the same coordinate space as the point
	// passed to hit_pointer_target. The page chooses its extent and availability;
	// neither is derived from a widget or changed by the query.
	struct PointerTarget
	{
		RectangleF bounds;
		bool enabled = true;
	};

	// The first enabled target containing point, or no match. The returned index
	// is in the original span, including entries skipped before it. Left/top
	// edges are included; right/bottom edges are excluded.
	//
	// Nonfinite points miss. Targets with nonfinite coordinates, dimensions or
	// computed endpoints, or nonpositive dimensions, are ignored. Negative
	// origins are valid; rectangles are not normalised or expanded.
	//
	// Pure, allocation-free and safe for concurrent reads of unchanged targets.
	// The page owns coordinate transforms, focus, capture and gesture policy.
	std::optional<size_t> hit_pointer_target(std::span<const PointerTarget> targets,
		const Vector2F& point);
}

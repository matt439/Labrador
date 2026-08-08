#include "engine/ui/navigation.h"

#include "engine/ui/widget.h"

#include <cmath>

using namespace mattmath;

namespace artattack
{
	namespace
	{
		// Screen space: x grows right, y grows *down*. "up" is therefore
		// decreasing y, which is the one thing about this file worth reading
		// twice.
		struct Axis
		{
			// How far along the direction of travel a point sits. Larger is
			// further in the direction asked for.
			float primary;
			// How far off that line. Sign is discarded; only magnitude ranks.
			float cross;
		};

		Axis project(const Vector2F& point, Direction direction)
		{
			switch (direction)
			{
			case Direction::up:    return { -point.y, point.x };
			case Direction::down:  return { point.y, point.x };
			case Direction::left:  return { -point.x, point.y };
			case Direction::right: return { point.x, point.y };
			case Direction::none:
			default:               return { 0.0f, 0.0f };
			}
		}

		bool degenerate(const RectangleF& box)
		{
			return box.width <= 0.0f || box.height <= 0.0f;
		}
	}

	int nearest_in_direction(const RectangleF& from,
		Direction direction,
		const std::vector<RectangleF>& candidates,
		bool wrap)
	{
		if (direction == Direction::none)
		{
			return -1;
		}

		const Axis origin = project(from.center(), direction);

		int forward = -1;
		float forward_primary = 0.0f;
		float forward_cross = 0.0f;

		// The wrap answer is computed in the same pass rather than in a second
		// one with the direction flipped: "furthest the other way" is just the
		// smallest primary, and every candidate is already projected.
		int wrapped = -1;
		float wrapped_primary = 0.0f;
		float wrapped_cross = 0.0f;

		const int count = static_cast<int>(candidates.size());
		for (int i = 0; i < count; i++)
		{
			const RectangleF& candidate = candidates[i];
			if (degenerate(candidate) || candidate == from)
			{
				continue;
			}

			const Axis there = project(candidate.center(), direction);
			const float primary = there.primary - origin.primary;
			const float cross = std::abs(there.cross - origin.cross);

			if (primary > 0.0f)
			{
				if (forward < 0 || primary < forward_primary ||
					(primary == forward_primary && cross < forward_cross))
				{
					forward = i;
					forward_primary = primary;
					forward_cross = cross;
				}
			}
			// Strictly behind, which is what makes it a wrap. Candidates level
			// with `from` on the axis of travel are not eligible: in a
			// left-aligned column every row shares a centre x, so "left" has
			// nothing ahead of it and must have nothing behind it either.
			// Treating level-with as behind makes pressing left in a vertical
			// menu jump to an arbitrary row, which is worse than doing
			// nothing.
			else if (wrap && primary < 0.0f)
			{
				if (wrapped < 0 || primary < wrapped_primary ||
					(primary == wrapped_primary && cross < wrapped_cross))
				{
					wrapped = i;
					wrapped_primary = primary;
					wrapped_cross = cross;
				}
			}
		}

		if (forward >= 0)
		{
			return forward;
		}
		return wrap ? wrapped : -1;
	}

	UiWidget* nearest_in_direction(const UiWidget& from,
		Direction direction,
		const std::vector<UiWidget*>& candidates,
		bool wrap)
	{
		std::vector<RectangleF> boxes;
		boxes.reserve(candidates.size());
		for (const UiWidget* candidate : candidates)
		{
			// Null entries and `from` itself keep their slot, so the returned
			// index still lines up with the caller's list; a zero box is
			// skipped by the walk. `from` is excluded here, by pointer, rather
			// than left to the rectangle overload's by-value comparison - two
			// widgets can honestly share a box, and only one of them is you.
			boxes.push_back(candidate == nullptr || candidate == &from
				? RectangleF::ZERO
				: candidate->bounds());
		}

		const int index = nearest_in_direction(from.bounds(), direction, boxes,
			wrap);
		return index < 0 ? nullptr : candidates[static_cast<size_t>(index)];
	}
}

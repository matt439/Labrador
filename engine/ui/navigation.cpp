#include "engine/ui/navigation.h"

#include "engine/ui/widget.h"
#include "engine/math/vector2f.h"

#include <cmath>
#include <vector>

using namespace mattmath;

namespace artattack
{
	namespace
	{
		// Screen space: x grows right, y grows *down*. "up" is therefore
		// decreasing y, which is the one thing about this file worth reading
		// twice.
		Direction opposite(Direction direction)
		{
			switch (direction)
			{
			case Direction::up:    return Direction::down;
			case Direction::down:  return Direction::up;
			case Direction::left:  return Direction::right;
			case Direction::right: return Direction::left;
			case Direction::none:
			default:               return Direction::none;
			}
		}

		// Clear space between the two boxes in `direction`: how far you would
		// have to travel from `from`'s leading edge to reach `to`'s trailing
		// one. Negative when `to` is not ahead at all.
		//
		// Edges, not centres. A left-aligned column of labels with different
		// text lengths has different centre x values on every row, so a
		// centre-based test makes "right" from "Standard" find "Team
		// Deathmatch" one row down. By edges, every row in that column starts
		// at the same x and none of them clears another's right edge, so left
		// and right find nothing - which is what a vertical menu should do.
		float gap(const RectangleF& from, const RectangleF& to,
			Direction direction)
		{
			switch (direction)
			{
			case Direction::up:    return from.top() - to.bottom();
			case Direction::down:  return to.top() - from.bottom();
			case Direction::left:  return from.left() - to.right();
			case Direction::right: return to.left() - from.right();
			case Direction::none:
			default:               return -1.0f;
			}
		}

		// Distance between centres on the axis *across* the direction of
		// travel. Ranks candidates that are equally far ahead: in a grid it is
		// what makes "down" find the cell below rather than the one below and
		// across.
		float cross_distance(const RectangleF& from, const RectangleF& to,
			Direction direction)
		{
			const Vector2F a = from.center();
			const Vector2F b = to.center();
			switch (direction)
			{
			case Direction::up:
			case Direction::down:  return std::abs(b.x - a.x);
			case Direction::left:
			case Direction::right: return std::abs(b.y - a.y);
			case Direction::none:
			default:               return 0.0f;
			}
		}

		// The box's size along the axis of travel.
		float extent(const RectangleF& box, Direction direction)
		{
			switch (direction)
			{
			case Direction::up:
			case Direction::down:  return box.height;
			case Direction::left:
			case Direction::right: return box.width;
			case Direction::none:
			default:               return 0.0f;
			}
		}

		// How much overlap on the axis of travel still counts as "ahead".
		//
		// Requiring a candidate to clear `from` completely is too strict for
		// real menus: the end-of-match menu spaces its rows 85px apart in a
		// 48pt font, and a font whose line spacing came out slightly larger
		// than the gap would leave every row fractionally overlapping its
		// neighbour - and navigation would stop entirely rather than
		// degrading. Half of the smaller box is a wide margin against that
		// and still nowhere near enough to let a ragged-right column, whose
		// rows overlap almost completely on the horizontal axis, answer a
		// press to the left or right.
		float overlap_tolerance(const RectangleF& from, const RectangleF& to,
			Direction direction)
		{
			return 0.5f * std::min(extent(from, direction),
				extent(to, direction));
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

		// Nearest thing ahead.
		int forward = -1;
		float forward_gap = 0.0f;
		float forward_cross = 0.0f;

		// Furthest thing behind, which is where a wrap lands. Eligibility is
		// the same test with the direction reversed rather than "everything
		// that is not ahead" - otherwise pressing left in a vertical menu,
		// where nothing is ahead and nothing is behind, would still jump
		// somewhere.
		int wrapped = -1;
		float wrapped_gap = 0.0f;
		float wrapped_cross = 0.0f;

		const Direction back = opposite(direction);
		const int count = static_cast<int>(candidates.size());
		for (int i = 0; i < count; i++)
		{
			const RectangleF& candidate = candidates[i];
			if (degenerate(candidate) || candidate == from)
			{
				continue;
			}

			const float cross = cross_distance(from, candidate, direction);
			const float tolerance =
				-overlap_tolerance(from, candidate, direction);

			const float ahead = gap(from, candidate, direction);
			if (ahead >= tolerance)
			{
				if (forward < 0 || ahead < forward_gap ||
					(ahead == forward_gap && cross < forward_cross))
				{
					forward = i;
					forward_gap = ahead;
					forward_cross = cross;
				}
				continue;
			}

			if (!wrap)
			{
				continue;
			}
			const float behind = gap(from, candidate, back);
			if (behind >= tolerance)
			{
				if (wrapped < 0 || behind > wrapped_gap ||
					(behind == wrapped_gap && cross < wrapped_cross))
				{
					wrapped = i;
					wrapped_gap = behind;
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

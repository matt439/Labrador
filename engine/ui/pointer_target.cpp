#include "engine/ui/pointer_target.h"

#include <cmath>

namespace labrador
{
	std::optional<size_t> hit_pointer_target(std::span<const PointerTarget> targets,
		const Vector2F& point)
	{
		if (!std::isfinite(point.x) || !std::isfinite(point.y))
		{
			return std::nullopt;
		}
		for (size_t index = 0; index < targets.size(); ++index)
		{
			const PointerTarget& target = targets[index];
			const RectangleF& box = target.bounds;
			if (!target.enabled || !std::isfinite(box.x) || !std::isfinite(box.y) ||
				!std::isfinite(box.width) || !std::isfinite(box.height) ||
				box.width <= 0.0f || box.height <= 0.0f)
			{
				continue;
			}
			const float right = box.x + box.width;
			const float bottom = box.y + box.height;
			if (std::isfinite(right) && std::isfinite(bottom) &&
				point.x >= box.x && point.y >= box.y &&
				point.x < right && point.y < bottom)
			{
				return index;
			}
		}
		return std::nullopt;
	}
}

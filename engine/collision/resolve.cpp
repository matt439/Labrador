#include "engine/collision/resolve.h"

#include <cmath>

using mattmath::Vector2F;

namespace artattack
{
	Vector2F separation(const Vector2F& normal, float penetration)
	{
		return normal * -penetration;
	}

	Vector2F separation_along(const Vector2F& normal, float penetration,
		const Vector2F& axis)
	{
		const float along = Vector2F::dot(normal, axis);
		if (std::abs(along) <= mattmath::EPSILON)
		{
			return Vector2F::ZERO;
		}

		return axis * (-penetration / along);
	}
}

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

	Vector2F slide(const Vector2F& velocity, const Vector2F& normal)
	{
		const float into_surface = Vector2F::dot(velocity, normal);
		if (into_surface <= 0.0f)
		{
			return velocity;
		}

		return velocity - normal * into_surface;
	}
}

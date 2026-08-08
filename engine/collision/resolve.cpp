#include "engine/collision/resolve.h"

#include <cmath>
#include <stdexcept>
#include <string>

using mattmath::Vector2F;

namespace artattack
{
	Vector2F separation(const Vector2F& normal, float penetration)
	{
		return normal * -penetration;
	}

	namespace
	{
		void require_unit(const Vector2F& vector, const char* name)
		{
			// Squared, so this costs no square root. The tolerance is on the
			// squared length, which is twice as loose as one on the length -
			// deliberately, since the point is to catch a vector that was
			// never normalised, not to police the last bit of one that was.
			const float length_squared = Vector2F::dot(vector, vector);
			if (std::abs(length_squared - 1.0f) > 0.001f)
			{
				throw std::invalid_argument(
					std::string("separation_along: ") + name +
					" must be unit length, and its length is " +
					std::to_string(std::sqrt(length_squared)));
			}
		}
	}

	Vector2F separation_along(const Vector2F& normal, float penetration,
		const Vector2F& axis)
	{
		require_unit(normal, "normal");
		require_unit(axis, "axis");

		if (!(penetration > 0.0f))
		{
			// Written as "not greater than", so a NaN penetration is caught
			// here rather than multiplied into a position.
			throw std::invalid_argument(
				"separation_along: penetration must be greater than zero, and "
				"it is " + std::to_string(penetration));
		}

		const float along = Vector2F::dot(normal, axis);
		if (std::abs(along) < MIN_AXIS_ALIGNMENT)
		{
			throw std::invalid_argument(
				"separation_along: axis is too far from the normal to separate "
				"this contact - dot is " + std::to_string(along) +
				", which would need a translation " +
				std::to_string(std::abs(penetration / along)) +
				" long to close an overlap of " + std::to_string(penetration) +
				". Use separation() to move along the normal itself.");
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

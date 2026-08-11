#include "engine/math/scalar.h"

#include "engine/math/vector2f.h"

#include <cmath>

namespace mattmath
{

	float mattmath::clamp(float value, float min, float max)
	{
		if (value < min)
		{
			return min;
		}
		else if (value > max)
		{
			return max;
		}
		else
		{
			return value;
		}
	}
	int mattmath::clamp(int value, int min, int max)
	{
		if (value < min)
		{
			return min;
		}
		else if (value > max)
		{
			return max;
		}
		else
		{
			return value;
		}
	}
	bool mattmath::are_equal(float a, float b, float epsilon)
	{
		return fabs(a - b) < epsilon;
	}
	bool mattmath::are_equal(const Vector2F& a, const Vector2F& b, float epsilon)
	{
		return are_equal(a.x, b.x, epsilon) && are_equal(a.y, b.y, epsilon);
	}
	float mattmath::to_radians(float degrees)
	{
		return degrees * (PI / 180.0f);
	}
	float mattmath::to_degrees(float radians)
	{
		return radians * (180.0f / PI);
	}

	float mattmath::lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

}

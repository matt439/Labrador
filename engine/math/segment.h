#pragma once

#include "engine/math/vector2f.h"

namespace labrador
{
	struct Segment
	{
		Point2F point_0 = Point2F::ZERO;
		Point2F point_1 = Point2F::ZERO;

		Segment() = default;
		Segment(const Segment&) = default;
		Segment& operator=(const Segment&) = default;
		Segment(const Point2F& point_0,
			const Point2F& point_1);
		Segment(float x0, float y0, float x1, float y1);

		bool operator==(const Segment& other) const;
		bool operator!=(const Segment& other) const;

		Vector2F direction() const;

		float length() const;

		Point2F center() const;
	};
}

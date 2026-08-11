#pragma once

#include "engine/math/vector2f.h"

namespace mattmath
{
	struct Segment
	{
		mattmath::Point2F point_0 = mattmath::Point2F::ZERO;
		mattmath::Point2F point_1 = mattmath::Point2F::ZERO;

		Segment() = default;
		Segment(const Segment&) = default;
		Segment(const mattmath::Point2F& point_0,
			const mattmath::Point2F& point_1);
		Segment(float x0, float y0, float x1, float y1);

		bool operator==(const Segment& other) const;
		bool operator!=(const Segment& other) const;

		mattmath::Vector2F direction() const;

		float length() const;

		mattmath::Point2F center() const;
	};
}

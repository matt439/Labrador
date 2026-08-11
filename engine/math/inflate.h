#pragma once

#include "engine/math/vector2f.h"

namespace mattmath
{
	namespace detail
	{
		// Shared by Triangle::inflate and Quad::inflate, which is the whole
		// reason it is declared at all. It used to sit in an anonymous
		// namespace in matt_math.cpp, where both callers could reach it
		// because both lived in that one translation unit. They do not any
		// more, so the choice was to publish it once or to duplicate the
		// mitre solve - and duplicating a hundred lines of geometry that the
		// Shape::inflate contract depends on is the worse of the two.
		//
		// It is in detail because it is not the vocabulary: callers inflate a
		// shape, and the shape decides what that means. The contract this
		// implements is written once, above Shape::inflate.
		void inflate_convex_polygon(Vector2F* points, int count, float amount);
	}
}

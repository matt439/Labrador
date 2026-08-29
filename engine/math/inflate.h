#pragma once

#include "engine/math/vector2f.h"

namespace mattmath
{
	namespace detail
	{
		// Shared by Triangle::inflate and Quad::inflate, which is the whole
		// reason it is declared at all. Its two callers are in different
		// translation units, so an anonymous namespace cannot serve both and the
		// choice is to publish it once or to duplicate the mitre solve -
		// duplicating a hundred lines of geometry that the Shape::inflate
		// contract depends on being the worse of the two.
		//
		// It is in detail because it is not the vocabulary: callers inflate a
		// shape, and the shape decides what that means. The contract this
		// implements is written once, above Shape::inflate.
		void inflate_convex_polygon(Vector2F* points, int count, float amount);
	}
}

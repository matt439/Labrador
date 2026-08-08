#pragma once

#include "engine/math/matt_math.h"

namespace artattack
{
	// What the narrow phase knows about one overlap: the axis it happened on,
	// and how deep it is.
	//
	// This is the value PHILOSOPHY's Collision section names, and having it be
	// a value is most of the point. What it replaces returned one of eight
	// cardinal Vector2F directions - a classification, not a measurement - and
	// then a separate call re-derived the depth by cloning the shape, resolving
	// the clone and subtracting the centres.
	struct Manifold
	{
		// Unit length, pointing from the first shape towards the second.
		mattmath::Vector2F normal = mattmath::Vector2F::ZERO;

		// How far the two overlap along `normal`. Always greater than zero: a
		// manifold only exists where there is an overlap, and shapes that
		// merely touch do not overlap.
		float penetration = 0.0f;
	};
}

#pragma once

namespace labrador
{
	// A one-dimensional displacement budget for discrete collision detection.
	// This is exact for fixed-size axis-aligned boxes translating along one
	// shared coordinate axis, with strictly overlapping intervals on the
	// other axis throughout the step. Extents are their positive widths or
	// heights on the travel axis; displacement is a nonnegative relative
	// travel distance and the step duration is positive.
	//
	// Under those assumptions, the interval of travel with positive overlap
	// has length extent_a + extent_b. A shorter step cannot cross it entirely.
	// The boundary itself is unsafe: both sampled endpoints can merely touch,
	// and narrow_phase.h treats touching as no contact.
	//
	// Projecting arbitrary shapes onto a diagonal does NOT establish this
	// interval. An off-center corner crossing can have an arbitrarily short
	// contact interval despite large projected extents. For diagonal motion,
	// rotation, or other geometry these helpers are only a tuning heuristic;
	// a false can_tunnel result does not prove safety. Clients choosing the
	// discrete model (PHILOSOPHY T3) must constrain their content and motion,
	// or separately test the trajectories that matter.
	constexpr float max_safe_displacement(float extent_a, float extent_b)
	{
		return extent_a + extent_b;
	}

	// Whether the one-dimensional budget above is reached or exceeded.
	// NaN displacement answers true. This does not test an actual trajectory.
	constexpr bool can_tunnel(float displacement, float extent_a, float extent_b)
	{
		return !(displacement < max_safe_displacement(extent_a, extent_b));
	}

	// The boundary speed under the same assumptions, in world units/second.
	// A safe working speed is strictly less than this boundary.
	constexpr float max_safe_speed(float extent_a, float extent_b,
		float seconds_per_step)
	{
		return max_safe_displacement(extent_a, extent_b) / seconds_per_step;
	}
}

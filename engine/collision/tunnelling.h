#pragma once

namespace artattack
{
	// The rule that lets a discrete collision test be correct.
	//
	// Everything in engine/collision measures shapes where they are now. Two
	// objects that would have passed through one another between one step and
	// the next are never overlapping when anything looks, so the collision
	// does not merely arrive late - it does not happen at all. Ericson states
	// the condition in the opening of his dynamic-intersection chapter (5.5,
	// pp.214-215): a discrete test is sound only while the relative
	// displacement over one step stays below the combined extent of the two
	// objects along the direction of travel.
	//
	// PHILOSOPHY (T3) accepts this deliberately: "Projectile speeds get capped
	// so nothing tunnels, instead of building continuous collision detection."
	// That trade is a good one, and it is only honest if the cap is real. This
	// header is the cap - it turns a sentence in a design document into an
	// arithmetic a test can check, which is the whole difference between a
	// declined feature and an undiscovered bug.

	// The furthest one object may move, relative to another, in a single step
	// while a discrete overlap test can still be relied upon to notice them.
	//
	// `extent_a` and `extent_b` are the two objects' extents along the
	// direction of travel - for axis-aligned boxes moving on an axis, their
	// widths or heights on that axis. The sum is the distance over which the
	// two are in contact, so a step shorter than it cannot step over the
	// contact entirely.
	//
	// This is the exact boundary, not a safe working figure. A displacement
	// equal to it grazes: the objects touch at exactly one instant, which the
	// narrow phase treats as not overlapping (see narrow_phase.h - touching is
	// not overlapping). Callers wanting a margin should compare against a
	// fraction of it, and the fraction should be written down where the speeds
	// are.
	constexpr float max_safe_displacement(float extent_a, float extent_b)
	{
		return extent_a + extent_b;
	}

	// Whether an object moving `displacement` in one step can pass through an
	// object it should have hit.
	//
	// Written as the dangerous case rather than the safe one, so that a NaN
	// displacement answers "yes, this can tunnel" rather than sliding through
	// on a comparison that is false for every operand.
	constexpr bool can_tunnel(float displacement, float extent_a, float extent_b)
	{
		return !(displacement < max_safe_displacement(extent_a, extent_b));
	}

	// The fastest an object of extent `extent_a` may travel, in world units
	// per second, and still be caught against an object of extent `extent_b`
	// by a simulation stepping `seconds_per_step` at a time.
	//
	// This is the form tuning data wants: speeds are authored in units per
	// second, and the step is a property of the application, so the two only
	// meet here.
	constexpr float max_safe_speed(float extent_a, float extent_b,
		float seconds_per_step)
	{
		return max_safe_displacement(extent_a, extent_b) / seconds_per_step;
	}
}

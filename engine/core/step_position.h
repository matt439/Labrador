#pragma once

#include <cmath>
#include <cstdint>

namespace labrador
{
	// Where a frame falls between two fixed steps (Application::step_position).
	//
	// The simulation advances at a fixed step and the frame is drawn whenever
	// the display asks, so on a display faster than the step - 120 or 144 Hz
	// against 60 - several frames draw one step's state, and anything moving
	// repeats a position and then jumps. That is judder, and it is the reason
	// a frame rate above the step is invisible to anything drawn straight
	// from the last update.
	//
	// THE ENGINE INTERPOLATES NOTHING. It says how far past the last step the
	// frame is, and each drawable chooses what to do with that: draw where it
	// was, extrapolate along its velocity by fraction steps, or interpolate
	// from a position it kept one step back. Which is right is a property of
	// what is drawn - a cell on a grid must not be drawn between cells - and
	// so is the game's (T1).
	struct StepPosition
	{
		// Fixed steps run since the clock started: the number of update()
		// calls the shell has made.
		std::uint64_t step = 0;

		// How far the frame is past that step, as a fraction of one step: at
		// least 0 and less than 1. A reset_elapsed_time discards what the
		// clock held toward the next step, so the fraction starts again from
		// what the clock sees after it.
		float fraction = 0.0f;
	};

	// The StepPosition of a clock that has run `step` steps of `step_ticks`
	// each and holds `left_over_ticks` toward the next. left_over_ticks below
	// step_ticks is the clock's own invariant; one at or above it, or a
	// step_ticks of zero, answers a fraction of 0 rather than one outside the
	// contract. A valid ratio that rounds to 1 answers the nearest float below 1.
	inline StepPosition step_position(std::uint64_t step,
		std::uint64_t left_over_ticks, std::uint64_t step_ticks)
	{
		StepPosition position;
		position.step = step;

		if (step_ticks != 0 && left_over_ticks < step_ticks)
		{
			// Rounding the ratio to float can reach 1 even with ticks left
			// before the next step. Keep the published fraction below it.
			position.fraction = static_cast<float>(
				static_cast<double>(left_over_ticks) /
				static_cast<double>(step_ticks));
			if (position.fraction >= 1.0f)
			{
				position.fraction = std::nextafter(1.0f, 0.0f);
			}
		}

		return position;
	}
}

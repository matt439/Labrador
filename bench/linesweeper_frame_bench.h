#pragma once

#include "bench/linesweeper_frame_result.h"

namespace linesweeper_frame_bench
{
	// Whether the benchmark window is shown. A hidden window is for
	// instrumented runs: it supplies no display-pacing evidence.
	enum class WindowVisibility
	{
		shown,
		hidden,
	};

	// Runs the LineSweeper frame workload `options` describes, warm-up and
	// retained sample, and returns the retained samples, the render device and
	// the measurement class. Writes nothing; the caller labels and publishes
	// the result.
	//
	// Throws std::runtime_error when the build is not Release, the CPU lacks
	// the renderer's instruction set, the device is not a hardware or null
	// one, the window's client area is not 1280x720, the device is lost, or
	// the window closes before the sample completes.
	Result measure_frames(const Options& options, WindowVisibility visibility);
}
